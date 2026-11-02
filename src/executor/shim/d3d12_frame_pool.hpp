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

    void         bind(ID3D12Device *device);
    StagedFrame *stage(ID3D12CommandQueue *queue, ID3D12Resource *source);

private:
    class Frame final : public StagedFrame {
    public:
        Frame();
        ~Frame();

        bool    claim();
        bool    idle() const;
        bool    holds(uint64_t bytes) const;
        bool    prepare(ID3D12Device *device, uint64_t bytes, const void *resolved);
        bool    copy(ID3D12CommandQueue *queue, ID3D12Resource *source, const void *footprint, uint32_t width,
                     uint32_t height, bool resolve);
        void    drop();

    private:
        Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) override;
        void    unmap() override;
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
        ID3D12Device              *device_;
    };

    Frame *claim(uint64_t bytes);

    ID3D12Device *device_;
    FramePool<Frame> frames_;
};

}

#endif
