#include "executor/shim/d3d9_frame_copier.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9FrameCopier::D3d9FrameCopier() : lane_(), copies_()
{
    AV_SCOPE("D3d9FrameCopier::D3d9FrameCopier");
}

D3d9FrameCopier::~D3d9FrameCopier()
{
    AV_SCOPE("D3d9FrameCopier::~D3d9FrameCopier");
    close();
}

bool D3d9FrameCopier::connect(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9FrameCopier::connect");
    if (!lane_.connect(device)) return false;
    if (copies_.open(lane_.table(), lane_.queue())) return true;
    close();
    return false;
}

void D3d9FrameCopier::close()
{
    AV_SCOPE("D3d9FrameCopier::close");
    /* Closing waits for the copies in flight and flushes DXVK's recorder, as a
     * copy does, so another frame's copy must not be recording meanwhile. */
    lane_.lock_device();
    copies_.close();
    lane_.unlock_device();
    lane_.release();
}

IDirect3DDevice9 *D3d9FrameCopier::device() const
{
    AV_SCOPE("D3d9FrameCopier::device");
    return lane_.device();
}

/* The recording thread is not the session's, so DXVK's device lock is taken
 * for what touches its recorder - the image's preparation and the flush - and
 * let go before the wait, which the session's own commands would stall on. */
StagedFrame::Copying D3d9FrameCopier::copy(IDirect3DSurface9 *surface, const AvFrameCopy &target, uint32_t wait_ms,
                                           uint64_t &value)
{
    AV_SCOPE("D3d9FrameCopier::copy");
    VkImage           image = VK_NULL_HANDLE;
    VkImageLayout     layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageCreateInfo described = {};
    described.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    value = 0u;
    bool              submitted = false;
    {
        lane_.lock_device();
        ScopeExit unlock([this] { lane_.unlock_device(); });
        submitted = lane_.surface_image(surface, image, layout, described) &&
                    copies_.submit(image, layout, described, target, value);
    }
    if (!submitted) return StagedFrame::Copying::declined;
    return copies_.wait(value, wait_ms) ? StagedFrame::Copying::copied : StagedFrame::Copying::timed_out;
}

bool D3d9FrameCopier::wait(uint64_t value, uint32_t wait_ms) const
{
    AV_SCOPE("D3d9FrameCopier::wait");
    return copies_.wait(value, wait_ms);
}

}  // namespace av
