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
    ~TileMappingSource() = default;
};

}

#endif
