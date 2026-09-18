/* The D3D9 proof, step by step (feature 047, US1): a device on the probe's
 * window, a frame cleared to a colour, the pixels read back and judged, and a
 * present.
 *
 * What it proves is DXVK's d3d9.dll on the guest's Vulkan loader on this ICD
 * on rvkgpu - the oldest of the three layers and the one whose fixed-function
 * paths DXVK emulates most, which is why it is worth running at all. */
#ifndef AV_D3D9_WORK_HPP
#define AV_D3D9_WORK_HPP

#include <stdint.h>

namespace av {

class D3d9Entry;
class ImageSink;
class ScenarioSteps;
struct D3d9WorkProbe;

class D3d9Work {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    static constexpr uint8_t  colour[4] = {64u, 128u, 191u, 255u};

    /* `images` keeps the frame as d3d9-clear. */
    D3d9Work(D3d9Entry &d3d9, ImageSink &images);
    ~D3d9Work();
    D3d9Work(const D3d9Work &) = delete;
    D3d9Work &operator=(const D3d9Work &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void     run(ScenarioSteps &steps, void *window);
    /* The adapter the device was made on, as its vendor and device ids
     * together: the vendor in the high half. 0 until the device is made. */
    uint64_t adapter() const;

    bool invariants_hold() const;

private:
    uint32_t coloured() const;

    D3d9Entry  &d3d9_;
    ImageSink  &images_;
    uint64_t    adapter_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];

    friend struct D3d9WorkProbe;
};

}  // namespace av

#endif  // AV_D3D9_WORK_HPP
