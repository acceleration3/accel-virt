#include "guest/probe/overlay_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/overlay_entry.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

OverlayWork::OverlayWork(OverlayEntry &dxgi) : dxgi_(dxgi), made_(false), mode_(0u)
{
    AV_SCOPE("OverlayWork::OverlayWork");
}

OverlayWork::~OverlayWork()
{
    AV_SCOPE("OverlayWork::~OverlayWork");
    if (made_) dxgi_.release();
}

void OverlayWork::run(ScenarioSteps &steps)
{
    AV_SCOPE("OverlayWork::run");
    if (steps.check_result("device", dxgi_.create_device()) != Status::ok) return;
    made_ = true;
    bool supported = false;
    if (steps.check_result("supports_overlays", dxgi_.supports_overlays(supported)) != Status::ok) return;
    /* What DXGI calls overlays is the video kind - YUV planes it can stretch
     * beside RGB ones (CDXGIOutput::GetMultiplaneOverlayCaps, dxgi.dll) - and
     * DWM promotes an RGB swap chain without them, so the steps go on. */
    steps.check_measured("overlays", supported ? Status::ok : Status::not_supported, supported ? 1u : 0u);
    uint32_t flags = 0u;
    if (steps.check_result("check_overlay_support", dxgi_.overlay_support(flags)) != Status::ok) return;
    steps.check_measured("overlay_support",
                         (flags & (support_direct | support_scaling)) != 0u ? Status::ok : Status::not_supported,
                         flags);
    promote(steps, "overlay_plane", window_x, window_y, window_width, window_height);
    promote(steps, "direct_flip", 0, 0, 0u, 0u);
}

void OverlayWork::promote(ScenarioSteps &steps, const char *step, int32_t x, int32_t y, uint32_t width,
                          uint32_t height)
{
    AV_SCOPE("OverlayWork::promote");
    if (steps.check_result(step, dxgi_.create_swap_chain(x, y, width, height)) != Status::ok) return;
    /* Only a refused present is a line of the report, not each of them. */
    for (uint32_t frame = 0u; frame < presents; frame++) {
        const int32_t presented = dxgi_.present(colour);
        if (presented < 0) {
            static_cast<void>(steps.check_result("present", presented));
            return;
        }
    }
    uint32_t mode = 0u;
    int32_t  asked = dxgi_.composition(mode);
    for (uint32_t retry = 0u; asked == disjoint && retry < disjoint_retries; retry++) {
        const int32_t presented = dxgi_.present(colour);
        asked = presented < 0 ? presented : dxgi_.composition(mode);
    }
    if (steps.check_result("frame_statistics", asked) != Status::ok) return;
    mode_ = mode;
    steps.check_measured(step, mode == mode_overlay ? Status::ok : Status::not_supported, mode);
}

uint32_t OverlayWork::mode() const
{
    AV_SCOPE("OverlayWork::mode");
    return mode_;
}

bool OverlayWork::invariants_hold() const
{
    AV_SCOPE("OverlayWork::invariants_hold");
    return made_ || mode_ == 0u;
}

}  // namespace av
