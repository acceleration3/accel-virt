/* One WriteTexture9's box, pitches and data judged against the subresource it
 * names, before a byte of it is copied: the box inside the level and on whole
 * elements (a block format's box starts on a block and ends on one or at the
 * level's edge), pitches no shorter than what they step over, and data that
 * reaches the last row of the last slice. Rows and bytes are counted in
 * elements, as the format lays them out (D3d9FormatLayout). */
#ifndef AV_D3D9_TEXTURE_WRITE_HPP
#define AV_D3D9_TEXTURE_WRITE_HPP

#include "proto/generated/d3d9/d3d9_wire_types.h"

#include <stdint.h>

namespace av {

class D3d9FormatLayout;

class D3d9TextureWrite {
public:
    D3d9TextureWrite(const d3d9proto::Box &box, uint32_t row_pitch, uint32_t slice_pitch, uint64_t data_size,
                     const D3d9FormatLayout &layout);

    /* Whether it lands inside a level of this size. */
    bool     fits(uint32_t width, uint32_t height, uint32_t depth) const;
    /* Rows of elements a slice of the box has, the bytes each takes, and its slices. */
    uint32_t rows() const;
    uint32_t row_bytes() const;
    uint32_t slices() const;

private:
    /* `first` to `last` of `extent` texels, on whole elements of `element` texels. */
    bool spans(uint32_t first, uint32_t last, uint32_t extent, uint32_t element) const;

    d3d9proto::Box          box_;
    uint32_t                row_pitch_;
    uint32_t                slice_pitch_;
    uint64_t                data_size_;
    const D3d9FormatLayout &layout_;
};

}  // namespace av

#endif  // AV_D3D9_TEXTURE_WRITE_HPP
