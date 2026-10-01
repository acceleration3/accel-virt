/* The frames Direct3D 11 stages for the present engine
 * (docs/shared-surfaces.md, "Readers"): a blt-model present's buffer copied on
 * the GPU into a staging texture, in the order of the commands that drew it,
 * which the engine then maps. The textures are kept and reused - a free one of
 * the frame's shape first - and a frame is the pool's again once
 * SharedSurfaces gives it back.
 *
 * Staging is a session's command, made under the device's lock. A frame is
 * read and given back from the present engine's thread, and takes the lock
 * itself only to map and unmap: given back unmapped - evicted - it takes none,
 * since SharedSurfaces may evict one under a session that holds it. */
#ifndef AV_D3D11_FRAME_POOL_HPP
#define AV_D3D11_FRAME_POOL_HPP

#include "device/surfaces/frame_claim.hpp"
#include "device/surfaces/frame_pool.hpp"
#include "device/surfaces/staged_frame.hpp"

#include <stdint.h>

struct ID3D11Texture2D;

namespace av {

class D3d11HostDevice;

class D3d11FramePool {
public:

    explicit D3d11FramePool(D3d11HostDevice &device);
    ~D3d11FramePool();
    D3d11FramePool(const D3d11FramePool &) = delete;
    D3d11FramePool &operator=(const D3d11FramePool &) = delete;

    /* Under the device's lock: subresource `subresource` of `source`, `width`
     * by `height` pixels of `format`, copied into a free frame and the copy
     * flushed to the GPU. Null when every frame is out, or the device will not
     * make a staging texture. */
    StagedFrame *stage(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                       uint32_t format);
    /* Under the device's lock, as it closes: every texture released. */
    void         clear();

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        void    bind(D3d11HostDevice &device);
        bool    out() const;
        /* Taken for a present (FramePool). */
        bool    claim();
        bool    fits(uint32_t width, uint32_t height, uint32_t format) const;
        /* Under the device's lock: the copy queued into this frame, which is
         * out until given back. False when no texture of that shape is made. */
        bool    take(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                     uint32_t format);
        /* Under the device's lock: the texture let go. */
        void    drop();
        Reading read(AvFrameView &view) override;
        void    release() override;

    private:
        D3d11HostDevice *device_;
        ID3D11Texture2D *texture_;
        uint32_t         width_;
        uint32_t         height_;
        uint32_t         format_;
        bool             mapped_;
        FrameClaim       claim_;
    };

    FramePool<Frame> frames_;
};

}  // namespace av

#endif  // AV_D3D11_FRAME_POOL_HPP
