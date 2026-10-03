/* One UpdateTileMappings as the runtime asked for it, read an element at a
 * time: its regions of a reserved resource's tiles, and the heap ranges those
 * tiles map to in order. The runtime's arrays are WDK types the portable half
 * cannot name and of any length, so TilePieces reads them through this -
 * implemented over the DDI's arrays in src/um/d3d and over plain arrays in
 * the tests. An absent array reads as the API defines it: a region at tile
 * (0, 0, 0) of subresource 0, of one tile (of the whole resource when neither
 * its start nor its size is given); a range with no flag, at heap tile 0, of
 * as many tiles as are left to map (~0). */
#ifndef AV_ICD_TILE_MAPPING_SOURCE_HPP
#define AV_ICD_TILE_MAPPING_SOURCE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av::icd {

class TileMappingSource {
public:
    TileMappingSource(const TileMappingSource &) = delete;
    TileMappingSource &operator=(const TileMappingSource &) = delete;

    virtual uint32_t               regions() const = 0;
    virtual d3d12proto::TileRegion region(uint32_t index) const = 0;
    virtual uint32_t               ranges() const = 0;
    virtual d3d12proto::TileRange  range(uint32_t index) const = 0;

protected:
    TileMappingSource() = default;
    /* Not virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~TileMappingSource() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_TILE_MAPPING_SOURCE_HPP
