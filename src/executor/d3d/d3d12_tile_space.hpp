/* The tiles of one reserved resource as the host tiled it, which every region
 * a guest names - in an update of its mappings, a copy of them, or a copy of
 * its tiles - must stay inside. vkd3d-proton walks a region tile by tile and
 * trusts the walk: CopyTiles indexes its own tile table by the index it
 * computes with no bound, and a box is walked by dividing by its width. So a
 * region is held here to what vkd3d-proton will compute of it - every tile's
 * index inside the resource, a box exactly its count of tiles - and to no
 * more tiles than the resource has, which bounds the walk. */
#ifndef AV_D3D12_TILE_SPACE_HPP
#define AV_D3D12_TILE_SPACE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct D3D12_SUBRESOURCE_TILING;

namespace av {

class D3d12TileSpace {
public:
    /* A resource of `tiles` tiles whose packed mips start at tile
     * `packed_start` (D3D12_PACKED_MIP_INFO). */
    D3d12TileSpace(uint32_t tiles, uint32_t packed_start);

    /* Whether `size` tiles from `start`, whose subresource is tiled as
     * `tiling`, are all tiles of the resource. */
    bool holds(const d3d12proto::TileCoordinate &start, const d3d12proto::TileSize &size,
               const D3D12_SUBRESOURCE_TILING &tiling) const;

private:
    /* The tile at (x, y, z) of a subresource tiled as `tiling`, the way
     * vkd3d-proton numbers it; false when the number is past 2^64. */
    bool index_of(uint64_t x, uint64_t y, uint64_t z, const D3D12_SUBRESOURCE_TILING &tiling, uint64_t &index) const;

    uint32_t tiles_;
    uint32_t packed_start_;
};

}  // namespace av

#endif  // AV_D3D12_TILE_SPACE_HPP
