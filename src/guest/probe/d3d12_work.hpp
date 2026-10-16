/* The D3D12 proof, step by step (feature 047, US2): a device, a queue, a
 * pipeline library made, serialized and made again from its blob, a render
 * target cleared to a colour through a command list, the pixels read back
 * and judged, and a bitmap of the frame.
 *
 * What it proves is vkd3d-proton's d3d12.dll on the guest's Vulkan loader on
 * this ICD on avgpu. The pixels are judged here rather than by the layer. */
#ifndef AV_D3D12_WORK_HPP
#define AV_D3D12_WORK_HPP

#include <stdint.h>

namespace av {

class D3d12Entry;
class ImageSink;
class ScenarioSteps;
struct D3d12WorkProbe;

class D3d12Work {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    static constexpr uint32_t pixel_bytes = 4u;
    /* The same colour the other probes clear to, red first. */
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    /* `images` keeps the frame as d3d12-clear. */
    D3d12Work(D3d12Entry &d3d12, ImageSink &images);
    ~D3d12Work();
    D3d12Work(const D3d12Work &) = delete;
    D3d12Work &operator=(const D3d12Work &) = delete;

    /* Every step through `steps`, stopping at the first that failed. */
    void     run(ScenarioSteps &steps);
    uint64_t adapter() const;
    uint32_t feature_level() const;
    uint32_t serialized_library() const;

    bool invariants_hold() const;

private:
    uint32_t coloured() const;

    D3d12Entry &d3d12_;
    ImageSink  &images_;
    uint64_t    adapter_;
    uint32_t    feature_level_;
    uint32_t    serialized_library_;
    bool        made_;
    bool        read_;
    uint8_t     frame_[pixels * pixel_bytes];

    friend struct D3d12WorkProbe;
};

}  // namespace av

#endif  // AV_D3D12_WORK_HPP
