#include "executor/d3d/d3d12_tile_space.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {
constexpr UINT packed_mip = 0xFFFFFFFFu;

}

D3d12TileSpace::D3d12TileSpace(uint32_t tiles, uint32_t packed_start) : tiles_(tiles), packed_start_(packed_start)
{
    AV_SCOPE("D3d12TileSpace::D3d12TileSpace");
}

bool D3d12TileSpace::holds(const d3d12proto::TileCoordinate &start, const d3d12proto::TileSize &size,
                           const D3D12_SUBRESOURCE_TILING &tiling) const
{
    AV_SCOPE("D3d12TileSpace::holds");
    if (size.tiles == 0u || size.tiles > tiles_) return false;
    uint64_t first = 0u;
    if (size.use_box == 0u)
        return index_of(start.x, start.y, start.z, tiling, first) && first <= uint64_t{tiles_} - size.tiles;
    if (size.width == 0u || size.height == 0u || size.depth == 0u) return false;
    if (uint64_t{size.width} * size.height * size.depth != size.tiles) return false;
    uint64_t last = 0u;
    return index_of(uint64_t{start.x} + size.width - 1u, uint64_t{start.y} + size.height - 1u,
                    uint64_t{start.z} + size.depth - 1u, tiling, last) &&
           last < tiles_;
}

bool D3d12TileSpace::index_of(uint64_t x, uint64_t y, uint64_t z, const D3D12_SUBRESOURCE_TILING &tiling,
                              uint64_t &index) const
{
    AV_SCOPE("D3d12TileSpace::index_of");
    if (tiling.StartTileIndexInOverallResource == packed_mip) {
        index = uint64_t{packed_start_} + x;
        return true;
    }
    const uint64_t rows = uint64_t{tiling.HeightInTiles} * z + y;
    uint64_t       across = 0u;
    if (__builtin_mul_overflow(uint64_t{tiling.WidthInTiles}, rows, &across)) return false;
    return !__builtin_add_overflow(uint64_t{tiling.StartTileIndexInOverallResource} + x, across, &index);
}

}
