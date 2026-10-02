/* The D3D11 proof, step by step (feature 047, US1): a device on the probe's
 * window, a render target of its back buffer, a frame cleared to a colour, the
 * pixels read back and judged, and a present.
 *
 * What it proves is the whole layer stack on this driver: DXVK's d3d11.dll on
 * the guest's Vulkan loader on this ICD on avgpu. Every step is reported as
 * the runner reads it, and the pixels are judged here rather than by the
 * layer: a frame that came back the wrong colour is a failure with the count
 * on the line. */
#ifndef AV_D3D11_WORK_HPP
#define AV_D3D11_WORK_HPP

#include <stdint.h>

namespace av {

class D3d11Entry;
class ImageSink;
class ScenarioSteps;
struct D3d11WorkProbe;

class D3d11Work {
public:
    /* The frame the probe clears and reads back. Small on purpose: a window
     * this size is what every probe here uses, and the readback is a member. */
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    /* The colour the frame is cleared to, red first; the same as the Vulkan
     * probe's, so a person comparing two bitmaps sees the same picture. */
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    /* `images` keeps the frame as d3d11-clear. */
    D3d11Work(D3d11Entry &d3d11, ImageSink &images);
    ~D3d11Work();
    D3d11Work(const D3d11Work &) = delete;
    D3d11Work &operator=(const D3d11Work &) = delete;

    /* Every step through `steps`, stopping at the first that failed: the
     * device on `window`, its adapter, the render target, the readback, the
     * pixels and the present. */
    void     run(ScenarioSteps &steps, void *window);
    /* The adapter the device was made on, for the runner to compare with the
     * driver's; 0 until the device is made. */
    uint64_t adapter() const;
    /* The feature level the device was made at; 0 until then. */
    uint32_t feature_level() const;

    /* Nothing is read back for a device that was never made, and no more
     * pixels are judged than the frame holds. */
    bool invariants_hold() const;

private:
    /* How many of the frame's pixels came back the colour, within the one
     * step of rounding a driver may choose. */
    uint32_t coloured() const;

    D3d11Entry &d3d11_;
    ImageSink  &images_;
    uint64_t    adapter_;
    uint32_t    feature_level_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];

    friend struct D3d11WorkProbe;
};

}  // namespace av

#endif  // AV_D3D11_WORK_HPP
