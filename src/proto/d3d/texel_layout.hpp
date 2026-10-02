/* How a format lays its texels out in memory, whichever runtime names it:
 * bytes per element and the element's width and height in texels - 4x4 for
 * the block-compressed formats, 2x1 for the packed 4:2:2 ones, 1x1 for
 * everything else. What a pitch, a lock's offset and a copy's rows are counted
 * in. A format with no layout of whole bytes has elements of no bytes, one
 * texel each: its rows are its texel rows and take no bytes, so a caller sees
 * the format is unknown rather than a wrong size.
 *
 * Made only from a runtime's format: D3d9FormatLayout, DxgiFormatLayout. */
#ifndef AV_TEXEL_LAYOUT_HPP
#define AV_TEXEL_LAYOUT_HPP

#include <stdint.h>

namespace av {

class TexelLayout {
public:
    /* Zero for a format with no layout of whole bytes per element. */
    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;
    /* The bytes one row of elements takes at `width` texels, and how many
     * rows of elements `height` texels are. */
    uint32_t row_bytes(uint32_t width) const;
    uint32_t rows(uint32_t height) const;
    /* The bytes a copy moves for one row: a format with no layout is still
     * copied, a byte a texel, rather than dropped. */
    uint32_t copied_row_bytes(uint32_t width) const;
    /* One four-byte element a texel: what a presented frame is staged as. */
    bool     word_texels() const;

protected:
    /* No layout: what a format the runtime's table does not name has. */
    TexelLayout();
    ~TexelLayout() = default;
    TexelLayout(const TexelLayout &) = default;
    TexelLayout &operator=(const TexelLayout &) = default;

    void lay_out(uint32_t bytes, uint32_t width, uint32_t height);

private:
    uint32_t bytes_;
    uint32_t width_;
    uint32_t height_;
};

}  // namespace av

#endif  // AV_TEXEL_LAYOUT_HPP
