#ifndef AV_ICD_TILE_PIECES_HPP
#define AV_ICD_TILE_PIECES_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av::icd {

class TileMappingSource;

class TilePieces {
public:
    static constexpr uint32_t most = 256u;

    explicit TilePieces(const TileMappingSource &source);
    TilePieces(const TilePieces &) = delete;
    TilePieces &operator=(const TilePieces &) = delete;

    [[nodiscard]] bool            next();
    const d3d12proto::TileRegion *regions() const;
    uint32_t                      region_count() const;
    const d3d12proto::TileRange  *ranges() const;
    uint32_t                      range_count() const;
    bool                          cut_short() const;

private:
    bool take_region();

    const TileMappingSource &source_;
    uint32_t                 region_;
    uint32_t                 range_;
    uint32_t                 range_used_;
    d3d12proto::TileRegion   regions_[most];
    d3d12proto::TileRange    ranges_[most];
    uint32_t                 region_count_;
    uint32_t                 range_count_;
    bool                     cut_short_;
};

}

#endif
