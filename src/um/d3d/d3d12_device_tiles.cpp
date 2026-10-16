/* Reserved resources and their tiles: a resource made with no memory of its
 * own, whose 64 KiB tiles a queue maps to heaps' tiles, and how the host
 * tiled it. The runtime works out a resource's tiles itself from the standard
 * tile shapes and asks the driver only how its mips are packed
 * (pfnGetMipPacking), and the host's vkd3d-proton tiles by the same shapes, so
 * the host's answer is the runtime's. An update of the mappings crosses in
 * pieces of what one command carries (TilePieces). */
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/d3d12_address_space.hpp"
#include "icd/d3d/tile_pieces.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_ddi_tile_source.hpp"
#include "um/d3d/d3d12_device.hpp"

namespace av::d3d12 {

namespace {

constexpr uint32_t buffer_dimension = 1u;
/* D3D12_HEAP_TYPE_DEFAULT: a reserved resource's tiles are the GPU's. */
constexpr uint32_t heap_default = 1u;

}  // namespace

HRESULT D3d12Device::reserve_resource(const D3D12DDIARG_CREATERESOURCE_0109 &args, const D3D12DDI_CLEAR_VALUES *clear,
                                      ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::reserve_resource");
    const d3d12proto::ResourceDesc desc = resource_desc(args);
    record_resource(desc, resource);
    resource.reserved = 1u;
    if (desc.dimension == buffer_dimension) {
        resource.address = addresses_.reserve(desc.width);
        if (resource.address == 0u) {
            LogLine(lines_).text("no guest address space for a reserved buffer of ").number(desc.width).text(" bytes").write();
            return E_OUTOFMEMORY;
        }
    }
    const bool made = d3d12proto::av_d3d12_CreateReservedResource12(
        wire(), resource.header.stamp.id(), desc, initial_state(args, heap_default), clear_value(clear), resource.address);
    return sent(made, "CreateReservedResource12") ? S_OK : E_OUTOFMEMORY;
}

const ResourceRecord &D3d12Device::tiling(ResourceRecord &resource)
{
    AV_SCOPE("D3d12Device::tiling");
    if (resource.reserved == 0u || resource.tiling_known != 0u) return resource;
    uint32_t tiles = 0u, packed_mips = 0u, packed_tiles = 0u;
    if (!sent(d3d12proto::av_d3d12_ResourceTiling12(wire(), resource.header.stamp.id(), &tiles, &packed_mips, &packed_tiles),
              "ResourceTiling12"))
        return resource;
    resource.tiles = tiles;
    resource.packed_mips = packed_mips;
    resource.packed_tiles = packed_tiles;
    resource.tiling_known = 1u;
    return resource;
}

VOID D3d12Device::get_mip_packing(D3D12DDI_HRESOURCE handle, UINT *packed, UINT *tiles)
{
    AV_SCOPE("D3d12Device::get_mip_packing");
    if (packed != nullptr) *packed = 0u;
    if (tiles != nullptr) *tiles = 0u;
    if (handle.pDrvPrivate == nullptr) return;
    const ResourceRecord &tiled = tiling(*static_cast<ResourceRecord *>(handle.pDrvPrivate));
    if (packed != nullptr) *packed = tiled.packed_mips;
    if (tiles != nullptr) *tiles = tiled.packed_tiles;
}

VOID D3d12Device::update_tile_mappings(D3D12DDI_HCOMMANDQUEUE queue_handle, D3D12DDI_HRESOURCE resource_handle,
                                       UINT region_count, const D3D12DDI_TILED_RESOURCE_COORDINATE *starts,
                                       const D3D12DDI_TILE_REGION_SIZE *sizes, D3D12DDI_HHEAP heap_handle,
                                       UINT range_count, const D3D12DDI_TILE_RANGE_FLAGS *flags, const UINT *offsets,
                                       const UINT *counts, D3D12DDI_TILE_MAPPING_FLAGS mapping)
{
    AV_SCOPE("D3d12Device::update_tile_mappings");
    if (queue_handle.pDrvPrivate == nullptr || resource_handle.pDrvPrivate == nullptr) return;
    const QueueRecord &queue = *static_cast<const QueueRecord *>(queue_handle.pDrvPrivate);
    ResourceRecord    &resource = *static_cast<ResourceRecord *>(resource_handle.pDrvPrivate);
    /* Only one region of neither a start nor a size needs the whole count. */
    const uint32_t     whole = starts == nullptr && sizes == nullptr ? tiling(resource).tiles : 0u;
    const DdiTileSource source(region_count, starts, sizes, range_count, flags, offsets, counts, whole);
    icd::TilePieces     pieces(source);
    const uint32_t      heap = id_of(heap_handle.pDrvPrivate);
    while (pieces.next())
        if (!sent(d3d12proto::av_d3d12_UpdateTileMappings12(wire(), queue.header.stamp.id(), resource.header.stamp.id(),
                                                            pieces.region_count(), pieces.regions(), heap,
                                                            pieces.range_count(), pieces.ranges(),
                                                            static_cast<uint32_t>(mapping)),
                  "UpdateTileMappings12"))
            return;
    if (pieces.cut_short()) unsupported("pfnUpdateTileMappings: a region of more ranges than one command carries");
}

VOID D3d12Device::copy_tile_mappings(D3D12DDI_HCOMMANDQUEUE queue_handle, D3D12DDI_HRESOURCE destination,
                                     const D3D12DDI_TILED_RESOURCE_COORDINATE *destination_start,
                                     D3D12DDI_HRESOURCE source, const D3D12DDI_TILED_RESOURCE_COORDINATE *source_start,
                                     const D3D12DDI_TILE_REGION_SIZE *size, D3D12DDI_TILE_MAPPING_FLAGS mapping)
{
    AV_SCOPE("D3d12Device::copy_tile_mappings");
    if (queue_handle.pDrvPrivate == nullptr || destination_start == nullptr || source_start == nullptr ||
        size == nullptr)
        return;
    const QueueRecord           &queue = *static_cast<const QueueRecord *>(queue_handle.pDrvPrivate);
    const d3d12proto::TileRegion to =
        DdiTileSource(1u, destination_start, size, 0u, nullptr, nullptr, nullptr, 0u).region(0u);
    const d3d12proto::TileRegion from =
        DdiTileSource(1u, source_start, size, 0u, nullptr, nullptr, nullptr, 0u).region(0u);
    static_cast<void>(sent(d3d12proto::av_d3d12_CopyTileMappings12(wire(), queue.header.stamp.id(),
                                                                    id_of(destination.pDrvPrivate), to.start,
                                                                    id_of(source.pDrvPrivate), from.start, to.size,
                                                                    static_cast<uint32_t>(mapping)),
                           "CopyTileMappings12"));
}

}  // namespace av::d3d12
