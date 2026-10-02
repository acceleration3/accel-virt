/* How one Direct3D 9 format (D3DFORMAT, FourCC codes included) lays its texels
 * out (TexelLayout): the DXTn and ATIn blocks are 4x4, the packed 4:2:2 ones
 * 2x1. What a lock's pitch and a texture write's rows are counted in, on both
 * ends of the wire. Unknown, A1, NULL and anything the table does not name
 * have no layout. */
#ifndef AV_D3D9_FORMAT_LAYOUT_HPP
#define AV_D3D9_FORMAT_LAYOUT_HPP

#include "proto/d3d/texel_layout.hpp"

#include <stdint.h>

namespace av {

class D3d9FormatLayout : public TexelLayout {
public:
    explicit D3d9FormatLayout(uint32_t format);

private:
    /* The layout of a FourCC format; none for one this does not know. */
    void four_cc();

    uint32_t format_;
};

}  // namespace av

#endif  // AV_D3D9_FORMAT_LAYOUT_HPP
