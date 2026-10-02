/* The frames Direct3D 12 stages for the present engine
 * (docs/shared-surfaces.md, "Readers"): a presented texture copied into a
 * readback buffer on the queue that presented it, after the work that drew
 * it and before anything that queue is given next, which the engine reads
 * once the frame's fence says the copy is done. A blt-model present - Direct3D
 * 9 on 12's, through dxgkrnl's redirected blt - is shown from these. A
 * multisampled one - Direct3D 9 on 12 presents its antialiased back buffer as
 * it is - is resolved into a texture of the frame's own first.
 *
 * Each frame has its own buffer, allocator, list and fence, so a copy queued
 * on one session's queue never waits for another's. One pool per GPU, beside
 * its Direct3D 12 device, so a frame outlives the session that staged it for
 * as long as SharedSurfaces keeps it. A session stages from its own thread and
 * the engine reads and gives back from its own: a frame is claimed and given
 * back atomically, and nothing else is shared. */
#ifndef AV_D3D12_FRAME_POOL_HPP
#define AV_D3D12_FRAME_POOL_HPP

#include "executor/d3d/frame_claim.hpp"
#include "executor/d3d/frame_pool.hpp"
#include "executor/d3d/staged_frame.hpp"

#include <stdint.h>

struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace av {

class D3d12FramePool {
public:

    D3d12FramePool();
    ~D3d12FramePool();
    D3d12FramePool(const D3d12FramePool &) = delete;
    D3d12FramePool &operator=(const D3d12FramePool &) = delete;

    /* The device frames are made on; null as it closes, which releases what
     * every frame made. */
    void         bind(ID3D12Device *device);
    /* `source`'s first subresource copied into a free frame on `queue`,
     * resolved first if it has samples. Null for a source this does not stage
     * - not a 2D texture of four-byte pixels, or a multisampled one of a
     * typeless format - for a queue that is not a direct one, when every
     * frame is out or still copying, or when the device makes none of what a
     * frame needs. */
    StagedFrame *stage(ID3D12CommandQueue *queue, ID3D12Resource *source);

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        /* Free and idle to out, atomically; false when it was out or the GPU
         * is still copying into it. */
        bool    claim();
        /* Whether the GPU has finished the last copy queued into it. */
        bool    idle() const;
        bool    holds(uint64_t bytes) const;
        /* What a copy of `bytes` needs, made on `device` where it is not
         * already: a mapped readback buffer, an allocator, a list, a fence,
         * and for `resolved` - a single-sample texture's description, null
         * for a source with one sample - that texture. */
        bool    prepare(ID3D12Device *device, uint64_t bytes, const void *resolved);
        /* The copy recorded and queued on `queue`, through the resolved
         * texture for a multisampled source, and the fence signalled after
         * it. `footprint` is the frame's image as GetCopyableFootprints placed
         * it. */
        bool    copy(ID3D12CommandQueue *queue, ID3D12Resource *source, const void *footprint, uint32_t width,
                     uint32_t height, bool resolve);
        void    drop();
        Reading read(AvFrameView &view) override;
        void    release() override;

    private:
        /* The resolve into resolved_ and its transitions, recorded. */
        void    record_resolve(ID3D12Resource *source);

        ID3D12Resource            *buffer_;
        ID3D12Resource            *resolved_;
        uint32_t                   resolved_format_;
        const uint8_t             *mapped_;
        uint64_t                   bytes_;
        ID3D12CommandAllocator    *allocator_;
        ID3D12GraphicsCommandList *list_;
        ID3D12Fence               *fence_;
        uint64_t                   value_;
        uint64_t                   offset_;
        uint32_t                   row_pitch_;
        uint32_t                   width_;
        uint32_t                   height_;
        FrameClaim                 claim_;
    };

    /* A free, idle frame, claimed: one already holding `bytes` first. */
    Frame *claim(uint64_t bytes);

    ID3D12Device *device_;
    FramePool<Frame> frames_;
};

}  // namespace av

#endif  // AV_D3D12_FRAME_POOL_HPP
