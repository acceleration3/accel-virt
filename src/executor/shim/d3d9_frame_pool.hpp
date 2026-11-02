/* The frames Direct3D 9 stages for the present engine (docs/shared-surfaces.md,
 * "Readers"): a blt-model present's render target copied on the GPU into a
 * single-sample render target the frame keeps (StretchRect, which resolves a
 * multisampled one too). The engine maps it to make its one CPU copy into the
 * window's pages (docs/compositor.md): the first read copies it into a
 * system-memory surface (GetRenderTargetData), with an event query after the
 * copy, and the engine maps that surface once the query says the GPU is done.
 * Where the present is to go into lent guest pages instead
 * (docs/compositor.md), the GPU copies `held_` straight there on the device's
 * own queue (D3d9FrameCopier) and nothing is read back.
 * The surfaces are kept and reused - a free one of the frame's device and
 * shape first - and a frame is the pool's again once SharedSurfaces gives it
 * back.
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
#include "device/shim/posix_lock.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "executor/shim/d3d9_frame_copier.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DQuery9;
struct IDirect3DSurface9;

namespace av {

class DriverLog;

class D3d9FramePool {
public:
    /* Guest devices whose frames this GPU's pool copies at once. */
    static constexpr uint32_t most_copiers = 4u;

    D3d9FramePool();
    ~D3d9FramePool();
    D3d9FramePool(const D3d9FramePool &) = delete;
    D3d9FramePool &operator=(const D3d9FramePool &) = delete;

    /* `source`, a render target of `device` `width` by `height` pixels of
     * the D3DFORMAT `format`, multisampled or not, copied into a free frame.
     * Null when every frame is out, or the device makes no surface or query
     * for it. */
    /* Where the pool says, once, that its copier slots are all taken. */
    void         bind(DriverLog &log);
    StagedFrame *stage(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                       uint32_t format, bool multisampled);
    /* As the GPU's Direct3D 9 closes: every frame not out let go. */
    void         clear();

private:
    /* A device's copier, and how many frames hold it. */
    struct Copier {
        D3d9FrameCopier copier_;
        uint32_t        uses_;
    };

    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        void    bind(D3d9FramePool &pool);

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

    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
        Copying copy_into(const AvFrameCopy &target, uint32_t wait_ms) override;
        /* How long drop waits for a copy still reading held_. */
        static constexpr uint32_t settle_ms = 1000u;
        /* The copy into system memory queued, behind the one into held_. */
        bool    copy_to_memory();

        D3d9FramePool     *pool_;
        IDirect3DDevice9  *device_;
        IDirect3DSurface9 *surface_;
        /* The frame on the GPU, single-sample whatever the source was. */
        IDirect3DSurface9 *held_;
        IDirect3DQuery9   *copied_;
        bool               multisampled_;
        bool               staged_;
        uint32_t           format_;
        bool               mapped_;
        /* What the lock handed back while mapped. */
        const uint8_t     *pixels_;
        uint32_t           pitch_;
        /* Held from the first copy to the frame's drop; asked for once. */
        D3d9FrameCopier   *copier_;
        bool               copier_asked_;
        /* The last copy submitted from held_; held_ goes only after it is reached. */
        uint64_t           copy_value_;
    };

    /* `device`'s copier, connected if it is not, and held for the caller;
     * null where the lane cannot be made or every slot is another device's. */
    D3d9FrameCopier *lend_copier(IDirect3DDevice9 *device);
    /* The caller no longer holds `copier`: closed once no frame does. */
    void             return_copier(D3d9FrameCopier *copier);

    DriverLog       *log_;
    bool             full_told_;
    /* Guards copiers_; each frame's own state is one thread's at a time. Both
     * come before the frames, which give their copier back as they go. */
    PosixLock        copiers_lock_;
    Copier           copiers_[most_copiers];
    FramePool<Frame> frames_;
};

}  // namespace av

#endif  // AV_D3D9_FRAME_POOL_HPP
