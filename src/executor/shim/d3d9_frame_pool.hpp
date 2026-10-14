/* The frames Direct3D 9 stages for the present engine (docs/shared-surfaces.md,
 * "Readers"): a blt-model present's render target copied on the GPU into a
 * single-sample render target the frame keeps (StretchRect, which resolves a
 * multisampled one too), which the GPU then writes into the window's own pages
 * through the device's Vulkan (write_frame, docs/compositor.md). Where it
 * cannot, the engine maps it: copied into a system-memory surface first
 * (GetRenderTargetData), with an event query after the copy, which the engine
 * maps once the query says the GPU is done. The surfaces are kept and reused -
 * a free one of the frame's device and shape first - and a frame is the
 * pool's again once SharedSurfaces gives it back.
 *
 * DXVK's LockRect waits for the GPU whatever its flags say, so the query is
 * what keeps the engine's read from blocking. A Direct3D 9 device is a guest
 * device's own, and created multithreaded: a frame keeps a reference to the
 * device it was copied on, which outlives the guest's device while the engine
 * still reads it. */
#ifndef AV_D3D9_FRAME_POOL_HPP
#define AV_D3D9_FRAME_POOL_HPP

#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "executor/shim/d3d9_vulkan_lane.hpp"
#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DQuery9;
struct IDirect3DSurface9;

namespace av {

class D3d9FramePool {
public:

    D3d9FramePool();
    ~D3d9FramePool();
    D3d9FramePool(const D3d9FramePool &) = delete;
    D3d9FramePool &operator=(const D3d9FramePool &) = delete;

    /* `source`, a render target of `device` `width` by `height` pixels of
     * the D3DFORMAT `format`, multisampled or not, copied into a free frame.
     * Null when every frame is out, or the device makes no surface or query
     * for it. */
    StagedFrame *stage(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                       uint32_t format, bool multisampled);
    /* As the GPU's Direct3D 9 closes: every frame not out let go. */
    void         clear();

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        bool    out() const;
        /* Taken for a present (FramePool). */
        bool    claim();
        bool    fits(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                     bool multisampled) const;
        /* The copy queued into this frame, which is out until given back.
         * False when no surface or query of that shape is made. */
        bool    take(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                     uint32_t format, bool multisampled);
        /* The surfaces and query of this shape made; false, with none kept,
         * where the device will not. */
        bool    make(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format, bool multisampled);
        void    drop();
        Reading read(AvFrameView &view) override;
        void    release() override;

    private:
        /* The copy into system memory queued, behind the one into held_. */
        bool    copy_to_memory();
        /* The lane and its timeline, made the first time a frame is written. */
        bool    lane_ready();

        IDirect3DDevice9  *device_;
        IDirect3DSurface9 *surface_;
        /* The frame on the GPU, single-sample whatever the source was. */
        IDirect3DSurface9 *held_;
        IDirect3DQuery9   *copied_;
        bool               multisampled_;
        bool               staged_;
        D3d9VulkanLane     lane_;
        VulkanTimeline     written_;
        uint64_t           writes_;
        uint32_t           width_;
        uint32_t           height_;
        uint32_t           format_;
        bool               mapped_;
        /* What the lock handed back while mapped. */
        const uint8_t     *pixels_;
        uint32_t           pitch_;
        FrameClaim         claim_;
    };

    FramePool<Frame> frames_;
};

}  // namespace av

#endif  // AV_D3D9_FRAME_POOL_HPP
