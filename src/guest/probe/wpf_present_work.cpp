#include "guest/probe/wpf_present_work.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/bmp_image.hpp"
#include "guest/probe/d3d9ex_entry.hpp"
#include "guest/probe/image_sink.hpp"
#include "guest/probe/probe_colour.hpp"
#include "guest/probe/window_frame.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

WpfPresentWork::WpfPresentWork(D3d9ExEntry &d3d9, WindowFrame &windows, ImageSink &images)
    : d3d9_(d3d9), windows_(windows), images_(images), made_(false), read_(false), looks_(0u), frame_{}
{
    AV_SCOPE("WpfPresentWork::WpfPresentWork");
}

WpfPresentWork::~WpfPresentWork()
{
    AV_SCOPE("WpfPresentWork::~WpfPresentWork");
    if (made_) d3d9_.release();
}

void WpfPresentWork::run(ScenarioSteps &steps, void *window)
{
    AV_SCOPE("WpfPresentWork::run");
    if (steps.check_result("wpf_device", d3d9_.create_device(window)) != Status::ok) return;
    made_ = true;
    if (steps.check_result("wpf_swap_chain", d3d9_.create_swap_chain(window, extent, extent)) != Status::ok) return;
    if (steps.check_result("wpf_draw", d3d9_.draw(colour)) != Status::ok) return;
    if (steps.check_result("wpf_present", d3d9_.present(extent, extent)) != Status::ok) return;
    const bool     seen = watch(window);
    const uint32_t right = seen ? coloured() : 0u;
    if (seen)
        images_.save("d3d9-wpf",
                     BmpImage(frame_, extent, extent, BmpImage::Rows::bottom_first, BmpImage::Order::blue_first));
    const Status shown = !seen ? Status::not_found : right == pixels ? Status::ok : Status::invalid_state;
    static_cast<void>(steps.check_measured("wpf_window", shown, right));
}

/* Nothing presents again: the frame either reaches the window or never does.
 * One read, then one after each blank until the last. */
bool WpfPresentWork::watch(void *window)
{
    AV_SCOPE("WpfPresentWork::watch");
    for (looks_ = 0u; looks_ < blanks; looks_++) {
        if (!windows_.read(window, extent, extent, frame_)) return false;
        read_ = true;
        if (coloured() == pixels || d3d9_.wait_for_blank() < 0) return true;
    }
    return windows_.read(window, extent, extent, frame_);
}

/* The alpha a window keeps is the desktop's, not the frame's. */
uint32_t WpfPresentWork::coloured() const
{
    AV_SCOPE("WpfPresentWork::coloured");
    return ProbeColour(window_colour, sizeof window_colour).matched(frame_, pixels, pixel_bytes);
}

bool WpfPresentWork::invariants_hold() const
{
    AV_SCOPE("WpfPresentWork::invariants_hold");
    if (!made_ && (read_ || looks_ != 0u)) return false;
    return looks_ <= blanks && coloured() <= pixels;
}

}  // namespace av
