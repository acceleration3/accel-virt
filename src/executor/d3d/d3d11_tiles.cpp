#include "executor/d3d/d3d11_tiles.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr uint32_t copy_linear_to_tiled = D3D11_TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE;
constexpr uint32_t copy_tiled_to_linear = D3D11_TILE_COPY_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER;
constexpr uint32_t copy_no_overwrite = D3D11_TILE_COPY_NO_OVERWRITE;

D3D11_TILED_RESOURCE_COORDINATE coordinate_of(const d3dproto::TileCoordinate &from)
{
    AV_SCOPE("coordinate_of");
    return D3D11_TILED_RESOURCE_COORDINATE{from.x, from.y, from.z, from.subresource};
}

bool box_matches(const d3dproto::TileSize &size)
{
    AV_SCOPE("box_matches");
    if (size.use_box == 0u) return true;
    return size.width <= 0xFFFFu && size.height <= 0xFFFFu && size.depth <= 0xFFFFu &&
           uint64_t{size.width} * size.height * size.depth == size.tiles;
}

D3D11_TILE_REGION_SIZE size_of(const d3dproto::TileSize &from)
{
    AV_SCOPE("size_of");
    D3D11_TILE_REGION_SIZE made = {};
    made.NumTiles = from.tiles;
    made.bUseBox = from.use_box != 0u;
    made.Width = from.width;
    made.Height = static_cast<UINT16>(from.height);
    made.Depth = static_cast<UINT16>(from.depth);
    return made;
}

}

D3d11Tiles::D3d11Tiles(const D3dObjectTable &objects) : objects_(objects)
{
    AV_SCOPE("D3d11Tiles::D3d11Tiles");
}

bool D3d11Tiles::tiling(ID3D11Device5 &device, uint32_t resource, uint32_t &tiles, uint32_t &packed_mips,
                        uint32_t &packed_tiles) const
{
    AV_SCOPE("D3d11Tiles::tiling");
    tiles = 0u;
    packed_mips = 0u;
    packed_tiles = 0u;
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (found == nullptr) return false;
    D3D11_PACKED_MIP_DESC packed = {};
    UINT                  entire = 0u;
    UINT                  none = 0u;
    device.GetResourceTiling(found, &entire, &packed, nullptr, &none, 0u, nullptr);
    tiles = entire;
    packed_mips = packed.NumPackedMips;
    packed_tiles = packed.NumTilesForPackedMips;
    return true;
}

bool D3d11Tiles::update_mappings(ID3D11DeviceContext4 &context, uint32_t resource, uint32_t region_count,
                                 const d3dproto::TileRegion *regions, uint32_t pool, uint32_t range_count,
                                 const d3dproto::TileRange *ranges, uint32_t flags) const
{
    AV_SCOPE("D3d11Tiles::update_mappings");
    ID3D11Resource *const found = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    ID3D11Buffer *const   tile_pool =
        pool == 0u ? nullptr : objects_.find_as<ID3D11Buffer>(pool, D3dObjectKind::resource);
    if (found == nullptr || (pool != 0u && tile_pool == nullptr)) return false;
    if (region_count == 0u || range_count == 0u || region_count > most_pieces || range_count > most_pieces)
        return false;
    D3D11_TILED_RESOURCE_COORDINATE starts[most_pieces];
    D3D11_TILE_REGION_SIZE          sizes[most_pieces];
    UINT                            range_flags[most_pieces];
    UINT                            offsets[most_pieces];
    UINT                            counts[most_pieces];
    for (uint32_t index = 0u; index < region_count; index++) {
        if (regions[index].size.tiles == 0u || !box_matches(regions[index].size)) return false;
        starts[index] = coordinate_of(regions[index].start);
        sizes[index] = size_of(regions[index].size);
    }
    for (uint32_t index = 0u; index < range_count; index++) {
        if (ranges[index].tiles == 0u) return false;
        range_flags[index] = ranges[index].flags;
        offsets[index] = ranges[index].pool_offset;
        counts[index] = ranges[index].tiles;
    }
    static_cast<void>(context.UpdateTileMappings(found, region_count, starts, sizes, tile_pool, range_count,
                                                  range_flags, offsets, counts, flags));
    return true;
}

bool D3d11Tiles::copy_mappings(ID3D11DeviceContext4 &context, uint32_t destination,
                               const d3dproto::TileCoordinate &destination_start, uint32_t source,
                               const d3dproto::TileCoordinate &source_start, const d3dproto::TileSize &size,
                               uint32_t flags) const
{
    AV_SCOPE("D3d11Tiles::copy_mappings");
    ID3D11Resource *const to = objects_.find_as<ID3D11Resource>(destination, D3dObjectKind::resource);
    ID3D11Resource *const from = objects_.find_as<ID3D11Resource>(source, D3dObjectKind::resource);
    if (to == nullptr || from == nullptr || size.tiles == 0u || !box_matches(size)) return false;
    const D3D11_TILED_RESOURCE_COORDINATE into = coordinate_of(destination_start);
    const D3D11_TILED_RESOURCE_COORDINATE out_of = coordinate_of(source_start);
    const D3D11_TILE_REGION_SIZE          region = size_of(size);
    static_cast<void>(context.CopyTileMappings(to, &into, from, &out_of, &region, flags));
    return true;
}

bool D3d11Tiles::copy(ID3D11DeviceContext4 &context, uint32_t resource, const d3dproto::TileCoordinate &start,
                      const d3dproto::TileSize &size, uint32_t buffer, uint64_t buffer_offset, uint32_t flags) const
{
    AV_SCOPE("D3d11Tiles::copy");
    ID3D11Resource *const tiled = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    ID3D11Buffer *const   linear = objects_.find_as<ID3D11Buffer>(buffer, D3dObjectKind::resource);
    if (tiled == nullptr || linear == nullptr || size.tiles == 0u || !box_matches(size)) return false;
    const D3D11_TILED_RESOURCE_COORDINATE at = coordinate_of(start);
    const D3D11_TILE_REGION_SIZE          region = size_of(size);
    const uint32_t kept = flags & (copy_linear_to_tiled | copy_tiled_to_linear | copy_no_overwrite);
    context.CopyTiles(tiled, &at, &region, linear, buffer_offset, kept);
    return true;
}

bool D3d11Tiles::update(ID3D11DeviceContext4 &context, uint32_t resource, const d3dproto::TileCoordinate &start,
                        const d3dproto::TileSize &size, uint32_t flags, const uint8_t *data,
                        uint64_t data_size) const
{
    AV_SCOPE("D3d11Tiles::update");
    ID3D11Resource *const tiled = objects_.find_as<ID3D11Resource>(resource, D3dObjectKind::resource);
    if (tiled == nullptr || size.tiles == 0u || !box_matches(size) ||
        data_size < uint64_t{size.tiles} * tile_bytes)
        return false;
    const D3D11_TILED_RESOURCE_COORDINATE at = coordinate_of(start);
    const D3D11_TILE_REGION_SIZE          region = size_of(size);
    context.UpdateTiles(tiled, &at, &region, data, flags & copy_no_overwrite);
    return true;
}

bool D3d11Tiles::resize(ID3D11DeviceContext4 &context, uint32_t pool, uint64_t bytes) const
{
    AV_SCOPE("D3d11Tiles::resize");
    ID3D11Buffer *const found = objects_.find_as<ID3D11Buffer>(pool, D3dObjectKind::resource);
    if (found == nullptr) return false;
    static_cast<void>(context.ResizeTilePool(found, bytes));
    return true;
}

bool D3d11Tiles::barrier(ID3D11DeviceContext4 &context, uint32_t before, uint32_t after) const
{
    AV_SCOPE("D3d11Tiles::barrier");
    D3dObjectKind kind = D3dObjectKind::none;
    IUnknown *const first = before == 0u ? nullptr : objects_.find_any(before, kind);
    IUnknown *const second = after == 0u ? nullptr : objects_.find_any(after, kind);
    if ((before != 0u && first == nullptr) || (after != 0u && second == nullptr)) return false;
    ID3D11DeviceChild *child_first = nullptr;
    ID3D11DeviceChild *child_second = nullptr;
    if (first != nullptr && FAILED(first->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void **>(&child_first))))
        return false;
    if (second != nullptr &&
        FAILED(second->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void **>(&child_second)))) {
        if (child_first != nullptr) child_first->Release();
        return false;
    }
    context.TiledResourceBarrier(child_first, child_second);
    if (child_first != nullptr) child_first->Release();
    if (child_second != nullptr) child_second->Release();
    return true;
}

}
