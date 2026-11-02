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
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    static constexpr uint8_t  colour[4] = {208u, 48u, 112u, 255u};
    static constexpr uint8_t  window_colour[3] = {colour[2], colour[1], colour[0]};
    static constexpr uint32_t blanks = 120u;

    WpfPresentWork(D3d9ExEntry &d3d9, WindowFrame &windows, ImageSink &images);
    ~WpfPresentWork();
    WpfPresentWork(const WpfPresentWork &) = delete;
    WpfPresentWork &operator=(const WpfPresentWork &) = delete;

    void run(ScenarioSteps &steps, void *window);

    bool invariants_hold() const;

private:
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

}

#endif
