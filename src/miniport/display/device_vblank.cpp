#include "miniport/display/device_vblank.hpp"

#include "miniport/display/display_mode.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/display/flip_view.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "proto/pages/vblank_record.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

DeviceVblank::DeviceVblank(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources)
    : platform_(platform), sources_(sources), notification_(platform, dxgkrnl), page_(platform),
      phase_(), layer_(), presented_layers_(), reporting_(false), suspended_(false), suspended_reporting_(false), sequence_(0u),
      reported_(0u), enables_(0u)
      {
          AV_SCOPE("DeviceVblank::DeviceVblank");
      }

Status DeviceVblank::initialize()
{
    AV_SCOPE("DeviceVblank::initialize");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::initialize");
    return page_.initialize();
}

void DeviceVblank::destroy()
{
    AV_SCOPE("DeviceVblank::destroy");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::destroy");
    reporting_ = false;
    page_.release();
    phase_.forget();
}

Status DeviceVblank::attach(VirtioGpuDevice &device)
{
    AV_SCOPE("DeviceVblank::attach");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::attach");
    if (page_.address() == 0u) return Status::invalid_state;
    uint8_t          request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.display_clock(page_.address()));
    uint8_t      answer[response_bytes] = {};
    uint32_t     written = 0u;
    const Status asked = device.ask_control(request, writer.length(), answer, sizeof answer, written);
    if (asked != Status::ok) return asked;
    GpuResponseReader response;
    const Status      decoded = response.decode(answer, written);
    if (decoded != Status::ok) return decoded;
    if (!response.ok()) return Status::not_supported;
    platform_.store_atomic(sequence_, 0u);
    phase_.forget();
    ask();
    return Status::ok;
}

Status DeviceVblank::enable()
{
    AV_SCOPE("DeviceVblank::enable");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::enable");
    if (page_.address() == 0u) return Status::invalid_state;
    VblankRecord blank;
    if (page_.read(target, blank) == Status::ok && blank.sequence() > platform_.load_atomic(sequence_))
        platform_.store_atomic(sequence_, blank.sequence());
    enables_++;
    reporting_ = true;
    ask();
    return Status::ok;
}

void DeviceVblank::disable()
{
    AV_SCOPE("DeviceVblank::disable");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::disable");
    reporting_ = false;
    ask();
}

void DeviceVblank::suspend()
{
    AV_SCOPE("DeviceVblank::suspend");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::suspend");
    suspended_reporting_ = reporting_;
    reporting_ = false;
    suspended_ = true;
    reported_ = 0u;
    enables_ = 0u;
}

Status DeviceVblank::resume()
{
    AV_SCOPE("DeviceVblank::resume");
    platform_.require_irql_at_most(Irql::passive, "DeviceVblank::resume");
    suspended_ = false;
    reporting_ = suspended_reporting_;
    suspended_reporting_ = false;
    ask();
    return page_.address() != 0u ? Status::ok : Status::invalid_state;
}

void DeviceVblank::ask()
{
    AV_SCOPE("DeviceVblank::ask");
    page_.ask(target, reporting_ ? 1u : sweep_every);
}

void DeviceVblank::tick(uint64_t now, bool latching)
{
    AV_SCOPE("DeviceVblank::tick");
    platform_.require_irql_at_most(Irql::dispatch, "DeviceVblank::tick");
    if (suspended_) return;
    VblankRecord   blank;
    const uint64_t seen = platform_.load_atomic(sequence_);
    if (page_.read(target, blank) != Status::ok || blank.sequence() <= seen) return;
    platform_.store_atomic(sequence_, blank.sequence());
    phase_.observe(blank.sequence(), now, blank.period_ns());
    if (reporting_) {
        if (report(sources_.shown(target, latching ? blank.latched() : 0u)) == Status::ok) reported_++;
        else platform_.log("avgpu: a vsync was refused");
    }
}

Status DeviceVblank::report(const FlipView &shown)
{
    AV_SCOPE("DeviceVblank::report");
    switch (shown.kind()) {
    case FlipView::Kind::planes_disabled: return notification_.report_multiplane_vsync(target, nullptr, 0u);
    case FlipView::Kind::base_plane:
        shown.layer(layer_);
        return notification_.report_multiplane_vsync(target, &layer_, 1u);
    case FlipView::Kind::presented_plane:
        return notification_.report_presented_vsync(target, presented_layers_, shown.layers(presented_layers_));
    default: break;
    }
    return notification_.report_vsync(target, shown.address());
}

uint32_t DeviceVblank::scan_line(const DisplayMode &mode, uint64_t now) const
{
    AV_SCOPE("DeviceVblank::scan_line");
    if (!phase_.known()) return mode.scan_line(now);
    return mode.scan_line_within(phase_.since_blank(now), phase_.period());
}

uint32_t DeviceVblank::reported() const
{
    AV_SCOPE("DeviceVblank::reported");
    return reported_;
}

uint32_t DeviceVblank::enabled_count() const
{
    AV_SCOPE("DeviceVblank::enabled_count");
    return enables_;
}

bool DeviceVblank::invariants_hold() const
{
    AV_SCOPE("DeviceVblank::invariants_hold");
    return phase_.invariants_hold() && (!suspended_ || !reporting_);
}

}
