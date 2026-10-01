/* How one Direct3D 9 format (D3DFORMAT, FourCC codes included) lays its texels
 * out in memory: bytes per element and the element's width and height in
 * texels - 4x4 for the DXTn and ATIn blocks, 2x1 for the packed 4:2:2 ones,
 * 1x1 for everything else. What a lock's pitch and a texture write's rows are
 * counted in, on both ends of the wire. */
#ifndef AV_D3D9_FORMAT_LAYOUT_HPP
#define AV_D3D9_FORMAT_LAYOUT_HPP

#include <stdint.h>

namespace av {

class D3d9FormatLayout {
public:
    explicit D3d9FormatLayout(uint32_t format);

    /* Zero for a format with no memory of whole bytes per element: unknown,
     * A1, NULL, or anything the table does not name. */
    uint32_t element_bytes() const;
    uint32_t element_width() const;
    uint32_t element_height() const;
    /* The bytes one row of elements takes at `width` texels, and how many
     * rows of elements `height` texels are. */
    uint32_t row_bytes(uint32_t width) const;
    uint32_t rows(uint32_t height) const;

private:
    /* A FourCC format's layout; nothing set for one this does not know. */
    void four_cc(uint32_t format);

    uint32_t bytes_;
    uint32_t width_;
    uint32_t height_;
};

}  // namespace av

#endif  // AV_D3D9_FORMAT_LAYOUT_HPP
