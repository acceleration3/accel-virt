#ifndef AV_D3D12_TILE_UPDATE_HPP
#define AV_D3D12_TILE_UPDATE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Heap;
struct ID3D12Resource;

namespace av {

class D3d12TileUpdate {
public:
    static constexpr uint32_t most = 2048u;

    D3d12TileUpdate();
    D3d12TileUpdate(const D3d12TileUpdate &) = delete;
    D3d12TileUpdate &operator=(const D3d12TileUpdate &) = delete;

    void     begin();
    bool     add_region(const d3d12proto::TileRegion &region);
    bool     add_range(const d3d12proto::TileRange &range, uint64_t heap_tiles);
    void     apply(ID3D12CommandQueue *queue, ID3D12Resource *resource, ID3D12Heap *heap, uint32_t flags);

    uint32_t regions() const;
    uint32_t ranges() const;

private:
    struct Start {
        uint32_t x_;
        uint32_t y_;
        uint32_t z_;
        uint32_t subresource_;
    };
    struct Size {
        uint32_t tiles_;
        int32_t  use_box_;
        uint32_t width_;
        uint16_t height_;
        uint16_t depth_;
    };

    Start    starts_[most];
    Size     sizes_[most];
    int32_t  flags_[most];
    uint32_t offsets_[most];
    uint32_t counts_[most];
    uint32_t regions_;
    uint32_t ranges_;
};

}

#endif
