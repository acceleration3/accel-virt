/* One Direct3D 9 device's way to copy its staged frames into lent guest pages
 * (FrameBufferCopy): its Vulkan lane on DXVK (D3d9VulkanLane) and the buffers
 * imported on its VkDevice. The device is held for as long as this is
 * connected. Any thread: the present engine's. */
#ifndef AV_D3D9_FRAME_COPIER_HPP
#define AV_D3D9_FRAME_COPIER_HPP

#include "device/abi/av_executor.h"
#include "executor/d3d/staged_frame.hpp"
#include "executor/shim/d3d9_vulkan_lane.hpp"
#include "executor/shim/frame_buffer_copy.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;

namespace av {

class D3d9FrameCopier {
public:
    D3d9FrameCopier();
    ~D3d9FrameCopier();
    D3d9FrameCopier(const D3d9FrameCopier &) = delete;
    D3d9FrameCopier &operator=(const D3d9FrameCopier &) = delete;

    /* `device`'s lane and queue; false where DXVK offers no interop or the
     * host has no memory for it. */
    [[nodiscard]] bool connect(IDirect3DDevice9 *device);
    void               close();
    IDirect3DDevice9  *device() const;

    /* `surface` of the device, copied as `target` asks and waited for up to
     * `wait_ms`. The device's lock is held while the copy is recorded and
     * submitted, never while it is waited for. declined where the surface or
     * the target cannot be copied this way. `value` is the copy's timeline
     * value once it is submitted, whether or not it was reached in time: the
     * surface may be let go only once `wait` has found it reached. */
    StagedFrame::Copying copy(IDirect3DSurface9 *surface, const AvFrameCopy &target, uint32_t wait_ms,
                              uint64_t &value);
    [[nodiscard]] bool   wait(uint64_t value, uint32_t wait_ms) const;

private:
    D3d9VulkanLane lane_;
    FrameBufferCopy copies_;
};

}  // namespace av

#endif  // AV_D3D9_FRAME_COPIER_HPP
