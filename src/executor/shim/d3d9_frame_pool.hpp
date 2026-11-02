#ifndef AV_D3D9_FRAME_POOL_HPP
#define AV_D3D9_FRAME_POOL_HPP

#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "executor/d3d/staged_frame.hpp"

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

    StagedFrame *stage(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                       uint32_t format, bool multisampled);
    void         clear();

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        bool    fits(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format,
                     bool multisampled) const;
        bool    take(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                     uint32_t format, bool multisampled);
        bool    make(IDirect3DDevice9 *device, uint32_t width, uint32_t height, uint32_t format, bool multisampled);
        void    drop();

    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
        bool    copy_to_memory();

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
    };

    FramePool<Frame> frames_;
};

}

#endif
