/* How one DXGI format lays its texels out (TexelLayout): the block-compressed
 * formats are 4x4, the packed 4:2:2 ones 2x1. What a readback needs to pack a
 * subresource's rows the way Direct3D's Map would. Unknown, the typeless
 * planar formats and values past the table have no layout. */
#ifndef AV_DXGI_FORMAT_LAYOUT_HPP
#define AV_DXGI_FORMAT_LAYOUT_HPP

#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av {

class DxgiFormatLayout : public TexelLayout {
public:
    explicit DxgiFormatLayout(uint32_t format);

    /* Whether DXGI_FORMAT has the value at all - up to A4B4G4R4_UNORM - which
     * is what a format the guest names is judged by before the host's
     * runtime is handed it. Typeless and planar formats are known and have
     * no layout. */
    bool known() const;

private:
    /* A packed 4:2:2 format: two texels to an element. */
    bool paired() const;
    bool block_compressed() const;

    uint32_t format_;
};

}  // namespace av

#endif  // AV_DXGI_FORMAT_LAYOUT_HPP
