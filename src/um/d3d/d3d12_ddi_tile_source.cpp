#include "um/d3d/d3d12_ddi_tile_source.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t every_tile_left = 0xFFFFFFFFu;

}

DdiTileSource::DdiTileSource(UINT region_count, const D3D12DDI_TILED_RESOURCE_COORDINATE *starts,
                             const D3D12DDI_TILE_REGION_SIZE *sizes, UINT range_count,
                             const D3D12DDI_TILE_RANGE_FLAGS *flags, const UINT *offsets, const UINT *counts,
                             uint32_t whole)
    : region_count_(region_count), starts_(starts), sizes_(sizes), range_count_(range_count), flags_(flags),
      offsets_(offsets), counts_(counts), whole_(whole)
{
    AV_SCOPE("DdiTileSource::DdiTileSource");
}

uint32_t DdiTileSource::regions() const
{
    AV_SCOPE("DdiTileSource::regions");
    return region_count_;
}

d3d12proto::TileRegion DdiTileSource::region(uint32_t index) const
{
    AV_SCOPE("DdiTileSource::region");
    d3d12proto::TileRegion made = {};
    if (starts_ != nullptr)
        made.start = d3d12proto::TileCoordinate{starts_[index].X, starts_[index].Y, starts_[index].Z,
                                                starts_[index].Subresource};
    if (sizes_ == nullptr) {
        made.size.tiles = starts_ != nullptr ? 1u : whole_;
        return made;
    }
    const D3D12DDI_TILE_REGION_SIZE &size = sizes_[index];
    made.size = d3d12proto::TileSize{size.NumTiles, size.UseBox ? 1u : 0u, size.Width, size.Height, size.Depth};
    return made;
}

uint32_t DdiTileSource::ranges() const
{
    AV_SCOPE("DdiTileSource::ranges");
    return range_count_;
}

d3d12proto::TileRange DdiTileSource::range(uint32_t index) const
{
    AV_SCOPE("DdiTileSource::range");
    return d3d12proto::TileRange{flags_ != nullptr ? static_cast<uint32_t>(flags_[index]) : 0u,
                                 offsets_ != nullptr ? offsets_[index] : 0u,
                                 counts_ != nullptr ? counts_[index] : every_tile_left};
}

}
