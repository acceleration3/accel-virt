#include "executor/d3d/d3d12_session.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_tile_space.hpp"
#include "executor/d3d/d3d12_tile_update.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"

namespace av {

namespace {

constexpr uint64_t tile_bytes = 64u * 1024u;
constexpr uint32_t no_hazard = 0x1u;
constexpr uint32_t copy_into_tiles = 0x2u;
constexpr uint32_t copy_out_of_tiles = 0x4u;
constexpr uint64_t word_bytes = 4u;
constexpr uint32_t most_box_side = 0xFFFFu;

D3D12_TILED_RESOURCE_COORDINATE host_coordinate(const d3d12proto::TileCoordinate &coordinate)
{
    AV_SCOPE("host_coordinate");
    return D3D12_TILED_RESOURCE_COORDINATE{coordinate.x, coordinate.y, coordinate.z, coordinate.subresource};
}

D3D12_TILE_REGION_SIZE host_size(const d3d12proto::TileSize &size)
{
    AV_SCOPE("host_size");
    D3D12_TILE_REGION_SIZE made = {};
    made.NumTiles = size.tiles;
    made.UseBox = size.use_box != 0u ? TRUE : FALSE;
    made.Width = size.width;
    made.Height = static_cast<UINT16>(size.height);
    made.Depth = static_cast<UINT16>(size.depth);
    return made;
}

}

bool D3d12Session::tiles_hold(ID3D12Resource *resource, const d3d12proto::TileCoordinate &start,
                              const d3d12proto::TileSize &size) const
{
    AV_SCOPE("D3d12Session::tiles_hold");
    if (size.use_box != 0u && (size.height > most_box_side || size.depth > most_box_side)) return false;
    UINT                     tiles = 0u;
    D3D12_PACKED_MIP_INFO    packed = {};
    UINT                     found = 1u;
    D3D12_SUBRESOURCE_TILING tiling = {};
    device_->device()->GetResourceTiling(resource, &tiles, &packed, nullptr, &found, start.subresource, &tiling);
    return found == 1u && D3d12TileSpace(tiles, packed.StartTileIndexInOverallResource).holds(start, size, tiling);
}

bool D3d12Session::on_ResourceTiling12(uint32_t resource, uint32_t &tiles, uint32_t &packed_mips,
                                       uint32_t &packed_tiles)
{
    AV_SCOPE("D3d12Session::on_ResourceTiling12");
    tiles = packed_mips = packed_tiles = 0u;
    ID3D12Resource *const tiled = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    if (device_ == nullptr || tiled == nullptr) return false;
    UINT                  count = 0u;
    D3D12_PACKED_MIP_INFO packed = {};
    device_->device()->GetResourceTiling(tiled, &count, &packed, nullptr, nullptr, 0u, nullptr);
    tiles = count;
    packed_mips = packed.NumPackedMips;
    packed_tiles = packed.NumTilesForPackedMips;
    return true;
}

bool D3d12Session::on_UpdateTileMappings12(uint32_t queue, uint32_t resource, uint32_t region_count,
                                           const d3d12proto::TileRegion *regions, uint32_t heap,
                                           uint32_t range_count, const d3d12proto::TileRange *ranges,
                                           uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_UpdateTileMappings12");
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    ID3D12Resource *const     tiled = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    ID3D12Heap *const         in = heap == 0u ? nullptr : objects_.find_as<ID3D12Heap>(heap, D3dObjectKind::heap12);
    if (on == nullptr || tiled == nullptr || (heap != 0u && in == nullptr) || region_count == 0u ||
        range_count == 0u || (flags & ~no_hazard) != 0u)
        return false;
    tile_update_.begin();
    for (uint32_t index = 0u; index < region_count; index++)
        if (!tiles_hold(tiled, regions[index].start, regions[index].size) || !tile_update_.add_region(regions[index]))
            return false;
    const uint64_t heap_tiles = in == nullptr ? 0u : in->GetDesc().SizeInBytes / tile_bytes;
    for (uint32_t index = 0u; index < range_count; index++)
        if (!tile_update_.add_range(ranges[index], heap_tiles)) return false;
    tile_update_.apply(on, tiled, in, flags);
    return true;
}

bool D3d12Session::on_CopyTileMappings12(uint32_t queue, uint32_t destination,
                                         const d3d12proto::TileCoordinate &destination_start, uint32_t source,
                                         const d3d12proto::TileCoordinate &source_start,
                                         const d3d12proto::TileSize &size, uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CopyTileMappings12");
    ID3D12CommandQueue *const on = objects_.find_as<ID3D12CommandQueue>(queue, D3dObjectKind::queue12);
    ID3D12Resource *const     to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    ID3D12Resource *const     from = objects_.find_as<ID3D12Resource>(source, D3dObjectKind::resource12);
    if (on == nullptr || to == nullptr || from == nullptr || (flags & ~no_hazard) != 0u ||
        !tiles_hold(to, destination_start, size) || !tiles_hold(from, source_start, size))
        return false;
    const D3D12_TILED_RESOURCE_COORDINATE to_start = host_coordinate(destination_start);
    const D3D12_TILED_RESOURCE_COORDINATE from_start = host_coordinate(source_start);
    const D3D12_TILE_REGION_SIZE          region = host_size(size);
    on->CopyTileMappings(to, &to_start, from, &from_start, &region, static_cast<D3D12_TILE_MAPPING_FLAGS>(flags));
    return true;
}

bool D3d12Session::on_CopyTiles12(uint32_t id, uint32_t resource, const d3d12proto::TileCoordinate &start,
                                  const d3d12proto::TileSize &size, uint32_t buffer, uint64_t buffer_offset,
                                  uint32_t flags)
{
    AV_SCOPE("D3d12Session::on_CopyTiles12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12Resource *const tiled = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    ID3D12Resource *const linear = objects_.find_as<ID3D12Resource>(buffer, D3dObjectKind::resource12);
    constexpr uint32_t    both_ways = copy_into_tiles | copy_out_of_tiles;
    if (recorded == nullptr || tiled == nullptr || linear == nullptr || (flags & ~(no_hazard | both_ways)) != 0u ||
        (flags & both_ways) == both_ways || !tiles_hold(tiled, start, size))
        return false;
    const D3D12_RESOURCE_DESC tiled_desc = tiled->GetDesc();
    const D3D12_RESOURCE_DESC linear_desc = linear->GetDesc();
    const uint64_t            texel = DxgiFormatLayout(static_cast<uint32_t>(tiled_desc.Format)).element_bytes();
    const uint64_t            step = texel > word_bytes ? texel : word_bytes;
    const bool                texture = tiled_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER;
    const uint64_t            bytes = uint64_t{size.tiles} * tile_bytes;
    if (linear_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || buffer_offset > linear_desc.Width ||
        bytes > linear_desc.Width - buffer_offset || (texture && buffer_offset % step != 0u))
        return false;
    const D3D12_TILED_RESOURCE_COORDINATE from = host_coordinate(start);
    const D3D12_TILE_REGION_SIZE          region = host_size(size);
    recorded->CopyTiles(tiled, &from, &region, linear, buffer_offset, static_cast<D3D12_TILE_COPY_FLAGS>(flags));
    return true;
}

}
