/* How one DXGI format lays its texels out in memory: bytes per element and the
 * element's width and height in texels - 4x4 for the block-compressed
 * formats, 2x1 for the packed 4:2:2 ones, 1x1 for everything else. What a
 * readback needs to pack a subresource's rows the way Direct3D's Map would. */
#ifndef AV_DXGI_FORMAT_LAYOUT_HPP
#define AV_DXGI_FORMAT_LAYOUT_HPP

#include <stdint.h>

namespace av {

class DxgiFormatLayout {
public:
    explicit DxgiFormatLayout(uint32_t format);

    /* Zero for a format with no fixed layout: unknown, typeless planar, or a
     * value past the table. */
    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;
    /* The bytes one row of elements takes at `width` texels, and how many
     * rows of elements `height` texels are. */
    uint32_t row_bytes(uint32_t width) const;
    uint32_t rows(uint32_t height) const;

private:
    uint32_t bytes_;
    uint32_t width_;
    uint32_t height_;
};

}  // namespace av

#endif  // AV_DXGI_FORMAT_LAYOUT_HPP
