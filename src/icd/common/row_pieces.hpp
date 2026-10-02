/* One slice of an image transfer cut into pieces one command can carry: a
 * region `width` x `height` texels of a layout's elements - a block format's
 * 4x4 blocks, an OpenCL image's pixels - whose rows lie `pitch` bytes apart in
 * the application's memory, in pieces of at most `most` bytes.
 *
 * As many whole rows of elements as fit go together; the last row of a piece
 * takes only its own bytes, every one before it a whole pitch. A row wider
 * than one piece goes alone, cut into runs of whole elements, so no command
 * is ever larger than its room - an overflowed command is a faulted context,
 * not a short transfer. Each piece names the texels it covers, clamped to the
 * region, and where its bytes start in the slice. Direct3D 9's WriteTexture9,
 * 11's UpdateSubresource and OpenCL's image reads and writes each send one per
 * piece. */
#ifndef AV_ICD_ROW_PIECES_HPP
#define AV_ICD_ROW_PIECES_HPP

#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av::icd {

class RowPieces {
public:
    /* `most` is at least one element's bytes; a layout of no bytes per
     * element, an empty region or a `most` smaller than one element sends
     * nothing. */
    RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most);

    /* On to the next piece; false once every element has been. */
    [[nodiscard]] bool next();
    /* The texels the piece covers, from the region's corner: its first row
     * and column, and one past its last - never past the region. */
    uint32_t top() const;
    uint32_t bottom() const;
    uint32_t left() const;
    uint32_t right() const;
    /* Where its bytes start in the slice's memory, and how many. */
    uint64_t offset() const;
    uint64_t bytes() const;
    /* The piece the transfer starts with, which alone may discard. */
    bool     first() const;

private:
    TexelLayout layout_;
    uint32_t    width_;
    uint32_t    height_;
    uint32_t    pitch_;
    /* The region in elements. */
    uint32_t    rows_;
    uint32_t    columns_;
    uint32_t    rows_per_piece_;
    uint32_t    columns_per_piece_;
    /* The piece: its first element row and column, and how many of each. */
    uint32_t    row_;
    uint32_t    column_;
    uint32_t    row_count_;
    uint32_t    column_count_;
};

}  // namespace av::icd

#endif  // AV_ICD_ROW_PIECES_HPP
