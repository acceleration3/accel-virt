/* How many thread groups one Dispatch may ask of a host GPU, on each axis: what
 * its Vulkan driver says. Direct3D 12's own limit is 65535 on each, but neither
 * its runtime nor a Windows driver holds an application to it, and Lightroom
 * Classic dispatches 69366 x 1 x 1. A GPU that takes more runs it as it would on
 * Windows; past what the GPU takes, the dispatch is undefined on the host, so it
 * stays refused. */
#ifndef AV_D3D12_DISPATCH_LIMIT_HPP
#define AV_D3D12_DISPATCH_LIMIT_HPP

#include <stdint.h>

struct ID3D12Device;

namespace av {

class D3d12DispatchLimit {
public:
    /* Direct3D 12's limit, until measured. */
    D3d12DispatchLimit();

    /* A device vkd3d-proton cannot say the Vulkan handles of keeps what it has. */
    void measure(ID3D12Device &device);
    bool admits(uint32_t x, uint32_t y, uint32_t z) const;

private:
    uint32_t most_x_;
    uint32_t most_y_;
    uint32_t most_z_;
};

}  // namespace av

#endif  // AV_D3D12_DISPATCH_LIMIT_HPP
