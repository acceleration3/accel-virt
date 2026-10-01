/* A placed footprint the guest names in a buffer: whether the bytes a copy
 * touches for a region of its texels lie inside the buffer. Rows are counted
 * in the format's elements - a block-compressed footprint has one row of
 * blocks for every four rows of texels, and its row pitch is a row of blocks
 * - and a footprint past Direct3D 12's own texture limits holds nothing. */
#ifndef AV_D3D12_FOOTPRINT_HPP
#define AV_D3D12_FOOTPRINT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av {

class D3d12Footprint {
public:
    D3d12Footprint(uint32_t format, uint32_t width, uint32_t height, uint32_t depth, uint32_t row_pitch,
                   uint64_t offset);

    /* All of it, as a region. */
    d3d12proto::Box whole() const;
    /* Whether `region` - (left, top, front) up to (right, bottom, back), inside
     * the footprint - lies in a buffer of `buffer_bytes`: from the offset to
     * the end of its last row in its last slice. A format with no fixed
     * layout is held to one row a texel, and its last row to its pitch. */
    bool            holds(const d3d12proto::Box &region, uint64_t buffer_bytes) const;

private:
    uint32_t format_;
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
    uint32_t row_pitch_;
    uint64_t offset_;
};

}  // namespace av

#endif  // AV_D3D12_FOOTPRINT_HPP
