#ifndef AV_UM_D3D12_DDI_TILE_SOURCE_HPP
#define AV_UM_D3D12_DDI_TILE_SOURCE_HPP

#include "icd/d3d/tile_mapping_source.hpp"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av::d3d12 {

class DdiTileSource final : public icd::TileMappingSource {
public:
    DdiTileSource(UINT region_count, const D3D12DDI_TILED_RESOURCE_COORDINATE *starts,
                  const D3D12DDI_TILE_REGION_SIZE *sizes, UINT range_count, const D3D12DDI_TILE_RANGE_FLAGS *flags,
                  const UINT *offsets, const UINT *counts, uint32_t whole);

    uint32_t               regions() const override;
    d3d12proto::TileRegion region(uint32_t index) const override;
    uint32_t               ranges() const override;
    d3d12proto::TileRange  range(uint32_t index) const override;

private:
    UINT                                     region_count_;
    const D3D12DDI_TILED_RESOURCE_COORDINATE *starts_;
    const D3D12DDI_TILE_REGION_SIZE         *sizes_;
    UINT                                     range_count_;
    const D3D12DDI_TILE_RANGE_FLAGS         *flags_;
    const UINT                              *offsets_;
    const UINT                              *counts_;
    uint32_t                                 whole_;
};

}

#endif
