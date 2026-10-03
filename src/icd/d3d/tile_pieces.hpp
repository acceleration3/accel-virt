/* An UpdateTileMappings in pieces of at most `most` regions and `most`
 * ranges, each an update of its own: sent in order, the pieces map what the
 * whole maps, since a later mapping of a tile wins over an earlier one either
 * way.
 *
 * A range runs on across regions, so a piece ends at a region's end and the
 * range that region ended in is cut there: its rest opens the next piece, its
 * heap offset moved on by the tiles already mapped (a range reusing one tile
 * keeps its one). The update ends where vkd3d-proton's walk of it does - when
 * either the regions or the ranges run out - and a range of no tiles ends as
 * soon as it starts. A region that alone needs more than `most` ranges is one
 * no piece holds: the update stops before it (cut_short). */
#ifndef AV_ICD_TILE_PIECES_HPP
#define AV_ICD_TILE_PIECES_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av::icd {

class TileMappingSource;

class TilePieces {
public:
    static constexpr uint32_t most = 256u;

    /* `source` outlives this. */
    explicit TilePieces(const TileMappingSource &source);
    TilePieces(const TilePieces &) = delete;
    TilePieces &operator=(const TilePieces &) = delete;

    /* On to the next piece; false once there is none, and from a region no
     * piece holds on. */
    [[nodiscard]] bool            next();
    const d3d12proto::TileRegion *regions() const;
    uint32_t                      region_count() const;
    const d3d12proto::TileRange  *ranges() const;
    uint32_t                      range_count() const;
    /* The update stopped at a region needing more than `most` ranges. */
    bool                          cut_short() const;

private:
    /* Appends the next region and the ranges its tiles take; false, appending
     * nothing, when they do not fit in this piece. */
    bool take_region();

    const TileMappingSource &source_;
    /* The next region, the range its first tile takes and how many tiles of
     * that range earlier regions took. */
    uint32_t                 region_;
    uint32_t                 range_;
    uint32_t                 range_used_;
    d3d12proto::TileRegion   regions_[most];
    d3d12proto::TileRange    ranges_[most];
    uint32_t                 region_count_;
    uint32_t                 range_count_;
    bool                     cut_short_;
};

}  // namespace av::icd

#endif  // AV_ICD_TILE_PIECES_HPP
