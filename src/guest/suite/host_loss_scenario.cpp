#include "guest/suite/host_loss_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/host_loss.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

namespace av {

namespace {

constexpr uint32_t format_b8g8r8a8 = 87u;
constexpr uint32_t bind_render_target = 0x20u;
constexpr uint32_t dimension_texture2d = 3u;
constexpr uint32_t rtv_texture2d = 4u;
constexpr uint32_t target_size = 16u;
constexpr uint32_t target_id = 0x100u;
constexpr uint32_t view_id = 0x101u;
constexpr uint32_t milliseconds_per_second = 1000u;

}  // namespace

HostLossScenario::HostLossScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "host_loss"), link_(kmt, search, steps_, SubApi::d3d),
      idle_staging_{}, other_staging_{}, next_staging_{}, other_connection_(kmt), next_connection_(kmt)
{
    AV_SCOPE("HostLossScenario::HostLossScenario");
}

Status HostLossScenario::make_device(KmtTransport &transport, uint32_t device)
{
    AV_SCOPE("HostLossScenario::make_device");
    uint32_t hresult = 1u, level = 0u;
    if (!d3dproto::av_d3d_CreateDevice11(transport, device, 0u, 0u, &hresult, &level) || hresult != 0u)
        return Status::invalid_state;
    hresult = 1u;
    if (!d3dproto::av_d3d_Finish11(transport, &hresult)) return Status::invalid_state;
    return hresult == 0u ? Status::ok : Status::invalid_state;
}

Status HostLossScenario::draw(KmtTransport &transport, uint32_t device)
{
    AV_SCOPE("HostLossScenario::draw");
    const Status made = make_device(transport, device);
    if (made != Status::ok) return made;
    d3dproto::ResourceDesc target = {};
    target.dimension = dimension_texture2d;
    target.width = target_size;
    target.height = target_size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = format_b8g8r8a8;
    target.sample_count = 1u;
    target.bind_flags = bind_render_target;
    d3dproto::ViewDesc view = {};
    view.resource = target_id;
    view.format = format_b8g8r8a8;
    view.dimension = rtv_texture2d;
    const d3dproto::Float4 colour = {{0.25f, 0.5f, 0.75f, 1.0f}};
    uint32_t               hresult = 1u;
    if (!d3dproto::av_d3d_CreateResource(transport, target_id, target, 0u, 0u, 0u) ||
        !d3dproto::av_d3d_CreateRenderTargetView(transport, view_id, view) ||
        !d3dproto::av_d3d_ClearRenderTargetView(transport, view_id, colour) ||
        !d3dproto::av_d3d_Finish11(transport, &hresult))
        return Status::invalid_state;
    return hresult == 0u ? Status::ok : Status::invalid_state;
}

uint32_t HostLossScenario::elapsed_ms(uint64_t started) const
{
    AV_SCOPE("HostLossScenario::elapsed_ms");
    uint64_t       frequency = 0u;
    const uint64_t now = kmt_.performance_counter(frequency);
    if (frequency == 0u) return 0u;
    const uint64_t milliseconds = (now - started) * milliseconds_per_second / frequency;
    return milliseconds > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(milliseconds);
}

Status HostLossScenario::await_fault(KmtTransport &transport, uint32_t &waited_ms)
{
    AV_SCOPE("HostLossScenario::await_fault");
    uint64_t       frequency = 0u;
    const uint64_t started = kmt_.performance_counter(frequency);
    waited_ms = 0u;
    while (!transport.host_faulted()) {
        waited_ms = elapsed_ms(started);
        if (waited_ms >= idle_seconds * milliseconds_per_second) return Status::invalid_state;
        kmt_.sleep_briefly();
    }
    waited_ms = elapsed_ms(started);
    return Status::ok;
}

void HostLossScenario::run()
{
    AV_SCOPE("HostLossScenario::run");
    ScopeExit closing([&] {
        link_.close();
        other_connection_.close();
        next_connection_.close();
    });
    if (link_.open() != Status::ok) return;
    KmtTransport &drawing = link_.transport();
    if (steps_.check("drawn", draw(drawing, 1u)) != Status::ok) return;
    KmtTransport idle(link_.connection(), SubApi::d3d, ScenarioTransport::ring_order, ScenarioTransport::reply_bytes,
                      idle_staging_, sizeof idle_staging_);
    Status made = idle.initialize();
    if (made == Status::ok) made = make_device(idle, 1u);
    if (steps_.check("idle_made", made) != Status::ok) return;
    KmtTransport other(other_connection_, SubApi::vulkan, ScenarioTransport::ring_order,
                       ScenarioTransport::reply_bytes, other_staging_, sizeof other_staging_);
    uint32_t answered = 0u;
    Status   answering = other_connection_.open(search_.adapter());
    if (answering == Status::ok) answering = other.initialize();
    if (answering == Status::ok) answering = other.nop(answered);
    if (steps_.check("other_before", answering) != Status::ok) return;

    const Status lost = HostLoss(drawing).send();
    if (lost == Status::not_supported) {
        steps_.note("lose", "the host keeps its devices: it was started without the device-loss layer");
        return;
    }
    if (steps_.check("lose", lost) != Status::ok) return;

    uint64_t       frequency = 0u;
    const uint64_t asked = kmt_.performance_counter(frequency);
    uint32_t       hresult = 0u;
    const bool     finished = d3dproto::av_d3d_Finish11(drawing, &hresult);
    static_cast<void>(steps_.check_measured("drawing_removed", finished ? Status::invalid_state : Status::ok,
                                            elapsed_ms(asked)));
    uint32_t waited_ms = 0u;
    static_cast<void>(steps_.check_measured("idle_removed", await_fault(idle, waited_ms), waited_ms));
    const uint32_t code = idle.host_fault_code();
    static_cast<void>(steps_.check_measured("idle_code", code == device_lost_code ? Status::ok : Status::invalid_state,
                                            code));
    steps_.check_refused("idle_refused", make_device(idle, 2u));
    static_cast<void>(steps_.check("other_kept", other.nop(answered)));

    KmtTransport next(next_connection_, SubApi::d3d, ScenarioTransport::ring_order, ScenarioTransport::reply_bytes,
                      next_staging_, sizeof next_staging_);
    Status recreated = next_connection_.open(search_.adapter());
    if (recreated == Status::ok) recreated = next.initialize();
    if (recreated == Status::ok) recreated = draw(next, 1u);
    static_cast<void>(steps_.check("recreated", recreated));
}

}  // namespace av
