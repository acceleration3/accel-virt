/* The frames Direct3D 11 stages for the present engine
 * (docs/shared-surfaces.md, "Readers"): a blt-model present's buffer copied on
 * the GPU into a texture of the pool's, in the order of the commands that drew
 * it. The engine maps it to make its one CPU copy into the window's pages
 * (docs/compositor.md): the first read copies it into a staging texture, made
 * the first time a frame of that shape is read, and the frame is copying
 * until the map no longer waits for the GPU. Where the present is to go into
 * lent guest pages instead, the GPU copies the held texture straight there on
 * DXVK's queue (FrameBufferCopy) and nothing is staged. The textures are kept and reused
 * - a free one of the frame's shape first - and a frame is the pool's again
 * once SharedSurfaces gives it back.
 *
 * Staging is a session's command, made under the device's lock. A frame is
 * read and given back from the present engine's thread, and takes the lock
 * itself only to map and unmap, and to record a GPU copy - never across the
 * wait for it: given back unmapped - evicted - it takes none, since
 * SharedSurfaces may evict one under a session that holds it. */
#ifndef AV_D3D11_FRAME_POOL_HPP
#define AV_D3D11_FRAME_POOL_HPP

#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "executor/shim/frame_buffer_copy.hpp"

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
    /* As the device closes: every texture released, frames still out
     * included, since a frame cannot release its texture once the device's
     * context is gone. Takes no lock: nothing presents from a closing device. */
    void         clear();

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        void    bind(D3d11HostDevice &device, FrameBufferCopy &copies);
        bool    fits(uint32_t width, uint32_t height, uint32_t format) const;
        /* Under the device's lock: the copy queued into this frame, which is
         * out until given back. False when no texture of that shape is made. */
        bool    take(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                     uint32_t format);
        /* Under the device's lock: the texture let go. */
        void    drop();
    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
        Copying copy_into(const AvFrameCopy &target, uint32_t wait_ms) override;
        /* Under the device's lock: the staging texture made if it is not, and
         * the frame copied into it. */
        bool    copy_to_staging();

        D3d11HostDevice *device_;
        FrameBufferCopy *copies_;
        ID3D11Texture2D *held_;
        ID3D11Texture2D *texture_;
        bool             staged_;
        uint32_t         format_;
        bool             mapped_;
        /* The last copy submitted from held_, and the opening of the copier
         * it is a value of; held_ goes only once it is done. */
        uint64_t         copy_value_;
        uint32_t         copy_epoch_;
        /* How long a frame let go waits for a copy still reading it. */
        static constexpr uint32_t settle_ms = 1000u;
        /* Whether held_ must be left alone: a copy has not finished. */
        bool    copy_pending();
    };

    /* Before the frames, which copy through it as they go. */
    FrameBufferCopy  copies_;
    FramePool<Frame> frames_;
};

}  // namespace av

#endif  // AV_D3D11_FRAME_POOL_HPP
