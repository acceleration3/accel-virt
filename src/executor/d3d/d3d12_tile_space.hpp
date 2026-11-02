#ifndef AV_D3D12_TILE_SPACE_HPP
#define AV_D3D12_TILE_SPACE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct D3D12_SUBRESOURCE_TILING;

namespace av {

class D3d12TileSpace {
public:
    D3d12TileSpace(uint32_t tiles, uint32_t packed_start);

    bool holds(const d3d12proto::TileCoordinate &start, const d3d12proto::TileSize &size,
               const D3D12_SUBRESOURCE_TILING &tiling) const;

private:
    bool index_of(uint64_t x, uint64_t y, uint64_t z, const D3D12_SUBRESOURCE_TILING &tiling, uint64_t &index) const;

    uint32_t tiles_;
    uint32_t packed_start_;
};

}

#endif
