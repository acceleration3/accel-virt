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
    static constexpr uint32_t most_copiers = 4u;

    D3d9FramePool();
    ~D3d9FramePool();
    D3d9FramePool(const D3d9FramePool &) = delete;
    D3d9FramePool &operator=(const D3d9FramePool &) = delete;

    void         bind(DriverLog &log);
    StagedFrame *stage(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                       uint32_t format, bool multisampled);
    void         clear();

private:
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
        bool    take(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                     uint32_t format, bool multisampled);
        bool    make(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format, bool multisampled);
        void    drop();

    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
        Copying copy_into(const AvFrameCopy &target, uint32_t wait_ms) override;
        static constexpr uint32_t settle_ms = 1000u;
        bool    copy_to_memory();

        D3d9FramePool     *pool_;
        IDirect3DDevice9  *device_;
        IDirect3DSurface9 *surface_;
        IDirect3DSurface9 *held_;
        IDirect3DQuery9   *copied_;
        bool               multisampled_;
        bool               staged_;
        uint32_t           format_;
        bool               mapped_;
        const uint8_t     *pixels_;
        uint32_t           pitch_;
        D3d9FrameCopier   *copier_;
        bool               copier_asked_;
        uint64_t           copy_value_;
    };

    D3d9FrameCopier *lend_copier(IDirect3DDevice9 *device);
    void             return_copier(D3d9FrameCopier *copier);

    DriverLog       *log_;
    bool             full_told_;
    PosixLock        copiers_lock_;
    Copier           copiers_[most_copiers];
    FramePool<Frame> frames_;
};

}

#endif
