#include "core/log/keyed_log.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/tile_pieces.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_ddi_tile_source.hpp"
#include "um/d3d/d3d11_device.hpp"

namespace av::d3d11 {

namespace {

constexpr uint32_t tile_bytes = 65536u;
constexpr uint32_t copy_linear_to_tiled = 0x2u;

d3dproto::TileCoordinate coordinate_of(const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE &from)
{
    AV_SCOPE("coordinate_of");
    return d3dproto::TileCoordinate{from.X, from.Y, from.Z, from.Subresource};
}

d3dproto::TileSize size_of(const D3DWDDM1_3DDI_TILE_REGION_SIZE &from)
{
    AV_SCOPE("size_of");
    return d3dproto::TileSize{from.NumTiles, from.bUseBox ? 1u : 0u, from.Width, from.Height, from.Depth};
}

}

const ResourceRecord &D3d11Device::tiling(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::tiling");
    if (resource.tiling_known != 0u) return resource;
    uint32_t tiles = 0u, packed_mips = 0u, packed_tiles = 0u;
    if (!d3dproto::av_d3d_ResourceTiling11(wire(), resource.stamp.id(), &tiles, &packed_mips, &packed_tiles)) {
        set_error(E_OUTOFMEMORY);
        return resource;
    }
    resource.tiles = tiles;
    resource.packed_mips = packed_mips;
    resource.packed_tiles = packed_tiles;
    resource.tiling_known = 1u;
    return resource;
}

VOID D3d11Device::get_mip_packing(D3D10DDI_HRESOURCE handle, UINT *packed, UINT *tiles)
{
    AV_SCOPE("D3d11Device::get_mip_packing");
    if (packed != nullptr) *packed = 0u;
    if (tiles != nullptr) *tiles = 0u;
    if (handle.pDrvPrivate == nullptr) return;
    const ResourceRecord &tiled = tiling(record(handle));
    if (packed != nullptr) *packed = tiled.packed_mips;
    if (tiles != nullptr) *tiles = tiled.packed_tiles;
}

VOID D3d11Device::update_tile_mappings(D3D10DDI_HRESOURCE resource_handle, UINT region_count,
                                       const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *starts,
                                       const D3DWDDM1_3DDI_TILE_REGION_SIZE *sizes, D3D10DDI_HRESOURCE pool_handle,
                                       UINT range_count, const UINT *flags, const UINT *offsets, const UINT *counts,
                                       UINT mapping)
{
    AV_SCOPE("D3d11Device::update_tile_mappings");
    if (resource_handle.pDrvPrivate == nullptr) return;
    ResourceRecord &resource = record(resource_handle);
    const uint32_t  whole = starts == nullptr && sizes == nullptr ? tiling(resource).tiles : 0u;
    const DdiTileSource source(region_count, starts, sizes, range_count, flags, offsets, counts, whole);
    icd::TilePieces     pieces(source);
    const uint32_t      pool = id_of(pool_handle.pDrvPrivate);
    while (pieces.next()) {
        d3dproto::TileRegion regions[icd::TilePieces::most] = {};
        d3dproto::TileRange  ranges[icd::TilePieces::most] = {};
        for (uint32_t index = 0u; index < pieces.region_count(); index++) {
            const d3d12proto::TileRegion &from = pieces.regions()[index];
            regions[index] = d3dproto::TileRegion{
                d3dproto::TileCoordinate{from.start.x, from.start.y, from.start.z, from.start.subresource},
                d3dproto::TileSize{from.size.tiles, from.size.use_box, from.size.width, from.size.height,
                                   from.size.depth}};
        }
        for (uint32_t index = 0u; index < pieces.range_count(); index++) {
            const d3d12proto::TileRange &from = pieces.ranges()[index];
            ranges[index] = d3dproto::TileRange{from.flags, from.heap_offset, from.tiles};
        }
        if (!d3dproto::av_d3d_UpdateTileMappings11(wire(), resource.stamp.id(), pieces.region_count(), regions, pool,
                                                   pieces.range_count(), ranges, mapping)) {
            set_error(E_OUTOFMEMORY);
            return;
        }
    }
    if (pieces.cut_short()) lines_.once("tile mappings whose regions outran their ranges were cut short");
}

VOID D3d11Device::copy_tile_mappings(D3D10DDI_HRESOURCE destination,
                                     const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *destination_start,
                                     D3D10DDI_HRESOURCE source,
                                     const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *source_start,
                                     const D3DWDDM1_3DDI_TILE_REGION_SIZE *size, UINT flags)
{
    AV_SCOPE("D3d11Device::copy_tile_mappings");
    if (destination_start == nullptr || source_start == nullptr || size == nullptr) return;
    if (!d3dproto::av_d3d_CopyTileMappings11(wire(), id_of(destination.pDrvPrivate), coordinate_of(*destination_start),
                                             id_of(source.pDrvPrivate), coordinate_of(*source_start), size_of(*size),
                                             flags))
        set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::copy_tiles(D3D10DDI_HRESOURCE resource, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *start,
                             const D3DWDDM1_3DDI_TILE_REGION_SIZE *size, D3D10DDI_HRESOURCE buffer, UINT64 offset,
                             UINT flags)
{
    AV_SCOPE("D3d11Device::copy_tiles");
    if (start == nullptr || size == nullptr) return;
    if (!d3dproto::av_d3d_CopyTiles11(wire(), id_of(resource.pDrvPrivate), coordinate_of(*start), size_of(*size),
                                      id_of(buffer.pDrvPrivate), offset, flags)) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    note_gpu_write((flags & copy_linear_to_tiled) != 0u ? resource : buffer);
}

VOID D3d11Device::update_tiles(D3D10DDI_HRESOURCE resource, const D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE *start,
                               const D3DWDDM1_3DDI_TILE_REGION_SIZE *size, const void *data, UINT flags)
{
    AV_SCOPE("D3d11Device::update_tiles");
    if (start == nullptr || size == nullptr || data == nullptr) return;
    const uint32_t per_command = static_cast<uint32_t>(piece_bytes() / tile_bytes);
    const uint32_t tiles = size->NumTiles;
    if (tiles <= per_command) {
        const uint64_t wanted[1] = {static_cast<uint64_t>(tiles) * tile_bytes};
        if (!wire().make_room_for_blobs(wanted, 1u)) lines_.once("a tile update larger than the arena");
        if (!d3dproto::av_d3d_UpdateTiles11(wire(), id_of(resource.pDrvPrivate), coordinate_of(*start), size_of(*size),
                                            flags, data, wanted[0]))
            set_error(E_OUTOFMEMORY);
        return;
    }
    if (size->bUseBox == 0u) {
        lines_.once("a tile update across subresources of more tiles than one command carries: its tail is dropped");
        D3DWDDM1_3DDI_TILE_REGION_SIZE head = *size;
        head.NumTiles = per_command;
        update_tiles(resource, start, &head, data, flags);
        return;
    }
    const uint32_t width = size->Width == 0u ? 1u : size->Width;
    const uint32_t height = size->Height == 0u ? 1u : size->Height;
    for (uint32_t tile = 0u; tile < tiles; tile++) {
        D3DWDDM1_3DDI_TILED_RESOURCE_COORDINATE at = *start;
        at.X += tile % width;
        at.Y += (tile / width) % height;
        at.Z += tile / (width * height);
        D3DWDDM1_3DDI_TILE_REGION_SIZE one = {};
        one.NumTiles = 1u;
        update_tiles(resource, &at, &one, static_cast<const uint8_t *>(data) + static_cast<size_t>(tile) * tile_bytes,
                     flags);
    }
}

VOID D3d11Device::tiled_resource_barrier(D3D11DDI_HANDLETYPE, void *before, D3D11DDI_HANDLETYPE, void *after)
{
    AV_SCOPE("D3d11Device::tiled_resource_barrier");
    if (!d3dproto::av_d3d_TiledResourceBarrier11(wire(), id_of(before), id_of(after))) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::resize_tile_pool(D3D10DDI_HRESOURCE pool, UINT64 bytes)
{
    AV_SCOPE("D3d11Device::resize_tile_pool");
    if (!d3dproto::av_d3d_ResizeTilePool11(wire(), id_of(pool.pDrvPrivate), bytes)) set_error(E_OUTOFMEMORY);
}

}
