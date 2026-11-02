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

    [[nodiscard]] bool connect(IDirect3DDevice9 *device);
    void               close();
    IDirect3DDevice9  *device() const;

    StagedFrame::Copying copy(IDirect3DSurface9 *surface, const AvFrameCopy &target, uint32_t wait_ms,
                              uint64_t &value);
    [[nodiscard]] bool   wait(uint64_t value, uint32_t wait_ms) const;

private:
    D3d9VulkanLane lane_;
    FrameBufferCopy copies_;
};

}

#endif
