#include "proto/d3d/dxgi_format_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* Bytes per element of every DXGI_FORMAT up to B4G4R4A4_UNORM (115), in
 * order; 0 where the format has no single-plane layout. Block-compressed
 * formats give their block's bytes. */
constexpr uint8_t element_sizes[] = {
    0, 16, 16, 16, 16, 12, 12, 12, 12, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 4, 4, 4, 8, 8,
    8, 16, 16, 16, 16, 16, 16, 8, 8, 8, 16, 16, 16, 2, 2, 4, 4, 4, 4, 4, 4, 4, 16, 16,
    16, 16, 16, 16, 4, 4, 8, 0, 0, 0, 0, 4, 8, 8, 0, 1, 1, 1, 2, 2};

constexpr uint32_t first_bc = 70u;     /* BC1_TYPELESS */
constexpr uint32_t last_bc1_3 = 84u;   /* BC5_SNORM */
constexpr uint32_t first_bc6 = 94u;    /* BC6H_TYPELESS */
constexpr uint32_t last_bc = 99u;      /* BC7_UNORM_SRGB */
constexpr uint32_t r8g8_b8g8 = 68u;
constexpr uint32_t g8r8_g8b8 = 69u;
constexpr uint32_t yuy2 = 107u;
constexpr uint32_t y210 = 108u;
constexpr uint32_t y216 = 109u;
constexpr uint32_t b4g4r4a4 = 115u;
constexpr uint32_t a4b4g4r4 = 191u;
constexpr uint32_t block = 4u;

}  // namespace

DxgiFormatLayout::DxgiFormatLayout(uint32_t format) : format_(format)
{
    AV_SCOPE("DxgiFormatLayout::DxgiFormatLayout");
    if (format_ == a4b4g4r4) {
        lay_out(2u, 1u, 1u);
        return;
    }
    if (format_ > b4g4r4a4) return;
    const uint32_t side = block_compressed() ? block : 1u;
    lay_out(element_sizes[format_], paired() ? 2u : side, side);
}

bool DxgiFormatLayout::known() const
{
    AV_SCOPE("DxgiFormatLayout::known");
    return format_ <= a4b4g4r4;
}

bool DxgiFormatLayout::paired() const
{
    AV_SCOPE("DxgiFormatLayout::paired");
    return format_ == r8g8_b8g8 || format_ == g8r8_g8b8 || format_ == yuy2 || format_ == y210 || format_ == y216;
}

bool DxgiFormatLayout::block_compressed() const
{
    AV_SCOPE("DxgiFormatLayout::block_compressed");
    return (format_ >= first_bc && format_ <= last_bc1_3) || (format_ >= first_bc6 && format_ <= last_bc);
}

}  // namespace av
