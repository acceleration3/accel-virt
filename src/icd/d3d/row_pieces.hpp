/* One depth slice of a texture write cut into pieces a command can carry:
 * whole rows of elements - a block-compressed format's rows of 4x4 blocks -
 * as many as `most` bytes hold at the source's pitch, at least one. Each
 * piece names the texel rows it covers and where its bytes start; the last
 * row of a piece takes only its line, not a whole pitch. Direct3D 9's
 * WriteTexture9 and 11's UpdateSubresource each send one per piece. */
#ifndef AV_D3D_ROW_PIECES_HPP
#define AV_D3D_ROW_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class RowPieces {
public:
    /* `rows` rows of elements over `texel_rows` texels, each `line` bytes
     * of the source's `pitch`. */
    RowPieces(uint32_t rows, uint32_t texel_rows, uint32_t line, uint32_t pitch, uint64_t most);

    /* On to the next piece; false once every row has been. */
    [[nodiscard]] bool next();
    /* The piece's texel rows, from the region's top: the first, and one
     * past the last - never past the region. */
    uint32_t top() const;
    uint32_t bottom() const;
    /* Where its bytes start in the slice's source, and how many. */
    uint64_t offset() const;
    uint64_t bytes() const;
    /* The piece the write starts with, which alone may discard. */
    bool     first() const;

private:
    uint32_t rows_;
    uint32_t texel_rows_;
    uint32_t line_;
    uint32_t pitch_;
    uint32_t per_piece_;
    /* Texel rows to an element row: 4 for a block-compressed format. */
    uint32_t texels_per_row_;
    uint32_t done_;
    uint32_t count_;
};

}  // namespace av::d3d

#endif  // AV_D3D_ROW_PIECES_HPP
