#include "core/display/synthetic_vsync.hpp"

#include "core/ddi/entry_points.hpp"
#include "core/display/display_sources.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SyntheticVsync::SyntheticVsync(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources)
    : platform_(platform), dxgkrnl_(dxgkrnl), sources_(sources), notification_(platform, dxgkrnl),
      timer_(platform, &AvVsyncTick, this), reporting_(false), running_(false), suspended_reporting_(false),
      layer_(), ticks_(0u), reported_(0u), enables_(0u), window_start_(0u)
      {
          AV_SCOPE("SyntheticVsync::SyntheticVsync");
      }

Status SyntheticVsync::initialize()
{
    AV_SCOPE("SyntheticVsync::initialize");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::initialize");
    return timer_.initialize();
}

Status SyntheticVsync::enable()
{
    AV_SCOPE("SyntheticVsync::enable");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::enable");
    enables_++;
    reporting_ = true;
    /* The window opens with the timer, whichever enable started it. */
    if (ticks_ == 0u) window_start_ = platform_.interrupt_time();
    if (running_) return Status::ok;
    const Status started = timer_.start(frame_period);
    running_ = started == Status::ok;
    return started;
}

/* The blank keeps its phase: only the reporting stops. */
void SyntheticVsync::disable()
{
    AV_SCOPE("SyntheticVsync::disable");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::disable");
    reporting_ = false;
}

void SyntheticVsync::destroy()
{
    AV_SCOPE("SyntheticVsync::destroy");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::destroy");
    reporting_ = false;
    running_ = false;
    timer_.destroy();
}

void SyntheticVsync::suspend()
{
    AV_SCOPE("SyntheticVsync::suspend");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::suspend");
    suspended_reporting_ = reporting_;
    reporting_ = false;
    running_ = false;
    timer_.stop();
    /* Interrupt time counts the sleep, so a window that straddles the
     * transition measures the sleep and not the period the timer kept. */
    ticks_ = 0u;
    reported_ = 0u;
    enables_ = 0u;
}

Status SyntheticVsync::resume()
{
    AV_SCOPE("SyntheticVsync::resume");
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::resume");
    if (!suspended_reporting_) return Status::ok;
    suspended_reporting_ = false;
    return enable();
}

void SyntheticVsync::tick()
{
    AV_SCOPE("SyntheticVsync::tick");
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::tick");
    ticks_++;
    if (reporting_) {
        if (report() == Status::ok) {
            reported_++;
            dxgkrnl_.notify_dpc();
            /* The adapter's DPC shows the newest image once a period: a flip
             * faster than the display waits for this tick to be shown. */
            static_cast<void>(dxgkrnl_.queue_dpc());
        } else {
            platform_.log("rvkgpu: a vsync was refused");
        }
    }
    if (ticks_ == ticks_per_report) report_rate();
}

/* Each flip kind is retired by one report only (VidSchiIsExpectedVSyncCookie,
 * dxgmms2.sys @14003a0d4): a CRTC_VSYNC naming the address for a
 * SetVidPnSourceAddress, and a CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY for a
 * multiplane flip - no layers for dxgmms2's disable-planes flip, which a
 * CRTC_VSYNC can never retire and whose queue then stalls. */
Status SyntheticVsync::report()
{
    AV_SCOPE("SyntheticVsync::report");
    switch (sources_.flip(target)) {
    case DisplaySources::Flip::planes_disabled: return notification_.report_multiplane_vsync(target, nullptr, 0u);
    case DisplaySources::Flip::base_plane:
        sources_.base_layer(target, layer_);
        return notification_.report_multiplane_vsync(target, &layer_, 1u);
    case DisplaySources::Flip::address: break;
    }
    return notification_.report_vsync(target, sources_.scanout_address(target));
}

uint32_t SyntheticVsync::reported() const
{
    AV_SCOPE("SyntheticVsync::reported");
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::reported");
    return reported_;
}

uint32_t SyntheticVsync::enabled_count() const
{
    AV_SCOPE("SyntheticVsync::enabled_count");
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::enabled_count");
    return enables_;
}

void SyntheticVsync::report_rate()
{
    AV_SCOPE("SyntheticVsync::report_rate");
    const uint64_t now = platform_.interrupt_time();
    char           bytes[96];
    TextBuffer     line(bytes, sizeof bytes);
    line.append("rvkgpu: vsync ");
    line.append_decimal(ticks_);
    line.append(" periods in ");
    line.append_decimal(now - window_start_);
    line.append(" ticks, ");
    line.append_decimal(reported_);
    line.append(" reported, enabled ");
    line.append_decimal(enables_);
    line.append(" times");
    platform_.log(line.text());
    window_start_ = now;
    ticks_ = 0u;
    reported_ = 0u;
    enables_ = 0u;
}

}  // namespace av
