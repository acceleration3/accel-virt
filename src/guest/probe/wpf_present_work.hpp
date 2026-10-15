/* WPF's presentation of a window, step by step, as its renderer drives
 * Direct3D 9Ex for a hardware-rendered window (RenderCapability tier 2): a
 * device made on a 1x1 swap chain of its own, the window's swap chain made
 * beside it - one buffer, D3DSWAPEFFECT_COPY - one frame drawn into that
 * buffer, and one present of it with a dirty region. A window whose content
 * does not change is presented exactly once, and never again.
 *
 * What it judges is the window itself, read off the desktop through
 * WindowFrame until it shows the frame or WaitForVBlank has counted out
 * `blanks`. A driver that loses the first present of a buffer passes every
 * application that presents again and leaves every WPF window black - the
 * host refused that first frame (SharedSurfaces), and Affinity's whole UI was
 * invisible. */
#ifndef AV_WPF_PRESENT_WORK_HPP
#define AV_WPF_PRESENT_WORK_HPP

#include <stdint.h>

namespace av {

class D3d9ExEntry;
class ImageSink;
class ScenarioSteps;
class WindowFrame;
struct WpfPresentWorkProbe;

class WpfPresentWork {
public:
    /* The probe window's side, which the swap chain is made at. */
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    /* What the frame is cleared to, red first: neither the D3D9 clear step's
     * colour - this runs on its window after it - nor a window's black. */
    static constexpr uint8_t  colour[4] = {208u, 48u, 112u, 255u};
    /* The same colour as a window's DIB holds it: blue, green, red. */
    static constexpr uint8_t  window_colour[3] = {colour[2], colour[1], colour[0]};
    /* How long the window is watched for the frame: two seconds of a 60 Hz
     * display, where a frame that arrives takes a blank or two. */
    static constexpr uint32_t blanks = 120u;

    /* `images` keeps the window as d3d9-wpf. */
    WpfPresentWork(D3d9ExEntry &d3d9, WindowFrame &windows, ImageSink &images);
    ~WpfPresentWork();
    WpfPresentWork(const WpfPresentWork &) = delete;
    WpfPresentWork &operator=(const WpfPresentWork &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void run(ScenarioSteps &steps, void *window);

    bool invariants_hold() const;

private:
    /* The window read until every pixel is the frame's, a blank between two
     * reads; false when it could not be read. */
    bool     watch(void *window);
    uint32_t coloured() const;

    D3d9ExEntry &d3d9_;
    WindowFrame &windows_;
    ImageSink   &images_;
    bool         made_;
    bool         read_;
    uint32_t     looks_;
    uint8_t      frame_[pixels * pixel_bytes];

    friend struct WpfPresentWorkProbe;
};

}  // namespace av

#endif  // AV_WPF_PRESENT_WORK_HPP
