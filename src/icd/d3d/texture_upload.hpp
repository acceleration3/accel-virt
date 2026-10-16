/* One box of a subresource sent to the host from the application's memory,
 * in pieces a command can carry: the whole box as one command when it fits,
 * else a depth slice at a time and, within one, as many rows of elements as
 * fit a piece - a row too wide for one in runs of whole elements (RowPieces).
 * Only the first piece may discard what the subresource held; the rest only
 * add to it. Direct3D 9's texture writes and Direct3D 11's UpdateSubresource
 * and unmaps are each one, sent through their own runtime's command
 * (UploadSink). A buffer is a box one row of one-byte elements high. */
#ifndef AV_D3D_TEXTURE_UPLOAD_HPP
#define AV_D3D_TEXTURE_UPLOAD_HPP

#include "proto/generated/d3d/d3d_wire_types.h"
#include "proto/resources/pitched_extent.hpp"
#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av::d3d {

class UploadSink;

class TextureUpload {
public:
    /* `box` - front to back slices, top to bottom rows and left to right
     * texels - of a subresource of `layout`, read from memory whose rows of
     * elements are `row_pitch` apart and slices `slice_pitch`, in pieces of at
     * most `most` bytes. */
    TextureUpload(const TexelLayout &layout, const d3dproto::Box &box, uint32_t row_pitch, uint32_t slice_pitch,
                  uint64_t most);

    /* The bytes the box reads: from its first row to the end of its last
     * slice's last row. 0 for an empty box or a format with no layout. */
    uint64_t bytes() const;
    /* Whether its memory can be read that way: a box with bytes, whose rows -
     * when there is more than one - are a whole line apart, and whose slices
     * a whole slice of rows. */
    bool     readable() const;
    /* Every piece to `sink`, in order. False when the box is not readable or
     * the sink stopped it. */
    [[nodiscard]] bool send(UploadSink &sink, const uint8_t *data) const;

private:
    PitchedExtent extent() const;
    uint32_t width() const;
    uint32_t height() const;
    uint32_t slices() const;
    [[nodiscard]] bool send_slice(UploadSink &sink, const uint8_t *data, uint32_t slice) const;

    TexelLayout   layout_;
    d3dproto::Box box_;
    uint32_t      row_pitch_;
    uint32_t      slice_pitch_;
    uint64_t      most_;
};

}  // namespace av::d3d

#endif  // AV_D3D_TEXTURE_UPLOAD_HPP
