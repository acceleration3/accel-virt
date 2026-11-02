#include "executor/d3d/d3d12_tile_update.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <stddef.h>

namespace av {

namespace {

constexpr uint32_t range_none = D3D12_TILE_RANGE_FLAG_NONE;
constexpr uint32_t range_null = D3D12_TILE_RANGE_FLAG_NULL;
constexpr uint32_t range_skip = D3D12_TILE_RANGE_FLAG_SKIP;
constexpr uint32_t range_reuse = D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE;

}

D3d12TileUpdate::D3d12TileUpdate() : starts_{}, sizes_{}, flags_{}, offsets_{}, counts_{}, regions_(0u), ranges_(0u)
{
    AV_SCOPE("D3d12TileUpdate::D3d12TileUpdate");
}

void D3d12TileUpdate::begin()
{
    AV_SCOPE("D3d12TileUpdate::begin");
    regions_ = 0u;
    ranges_ = 0u;
}

bool D3d12TileUpdate::add_region(const d3d12proto::TileRegion &region)
{
    AV_SCOPE("D3d12TileUpdate::add_region");
    if (regions_ == most) return false;
    const d3d12proto::TileCoordinate &start = region.start;
    const d3d12proto::TileSize       &size = region.size;
    starts_[regions_] = Start{start.x, start.y, start.z, start.subresource};
    sizes_[regions_] = Size{size.tiles, size.use_box != 0u ? 1 : 0, size.width, static_cast<uint16_t>(size.height),
                            static_cast<uint16_t>(size.depth)};
    regions_++;
    return true;
}

bool D3d12TileUpdate::add_range(const d3d12proto::TileRange &range, uint64_t heap_tiles)
{
    AV_SCOPE("D3d12TileUpdate::add_range");
    if (ranges_ == most || range.tiles == 0u) return false;
    const bool maps = range.flags == range_none || range.flags == range_reuse;
    if (!maps && range.flags != range_null && range.flags != range_skip) return false;
    const uint64_t used = range.flags == range_reuse ? 1u : range.tiles;
    if (maps && uint64_t{range.heap_offset} + used > heap_tiles) return false;
    flags_[ranges_] = static_cast<int32_t>(range.flags);
    offsets_[ranges_] = range.heap_offset;
    counts_[ranges_] = range.tiles;
    ranges_++;
    return true;
}

void D3d12TileUpdate::apply(ID3D12CommandQueue *queue, ID3D12Resource *resource, ID3D12Heap *heap, uint32_t flags)
{
    AV_SCOPE("D3d12TileUpdate::apply");
    using Coordinate = D3D12_TILED_RESOURCE_COORDINATE;
    using Region = D3D12_TILE_REGION_SIZE;
    static_assert(sizeof(Start) == sizeof(Coordinate) && offsetof(Start, x_) == offsetof(Coordinate, X) &&
                  offsetof(Start, y_) == offsetof(Coordinate, Y) && offsetof(Start, z_) == offsetof(Coordinate, Z) &&
                  offsetof(Start, subresource_) == offsetof(Coordinate, Subresource));
    static_assert(sizeof(Size) == sizeof(Region) && offsetof(Size, tiles_) == offsetof(Region, NumTiles) &&
                  offsetof(Size, use_box_) == offsetof(Region, UseBox) && sizeof(Size::use_box_) == sizeof(BOOL) &&
                  offsetof(Size, width_) == offsetof(Region, Width) && offsetof(Size, height_) == offsetof(Region, Height) &&
                  offsetof(Size, depth_) == offsetof(Region, Depth));
    static_assert(sizeof(int32_t) == sizeof(D3D12_TILE_RANGE_FLAGS) && sizeof(uint32_t) == sizeof(UINT));
    reinterpret_cast<ID3D12CommandQueueTileMappings *>(queue)->UpdateTileMappings(
        resource, regions_, reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE *>(starts_),
        reinterpret_cast<const D3D12_TILE_REGION_SIZE *>(sizes_), heap, ranges_,
        reinterpret_cast<const D3D12_TILE_RANGE_FLAGS *>(flags_), offsets_, counts_,
        static_cast<D3D12_TILE_MAPPING_FLAGS>(flags));
}

uint32_t D3d12TileUpdate::regions() const
{
    AV_SCOPE("D3d12TileUpdate::regions");
    return regions_;
}

uint32_t D3d12TileUpdate::ranges() const
{
    AV_SCOPE("D3d12TileUpdate::ranges");
    return ranges_;
}

}
