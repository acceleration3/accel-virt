#include "device/executor/dxgi_format_layout.hpp"

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

DxgiFormatLayout::DxgiFormatLayout(uint32_t format) : bytes_(0u), width_(1u), height_(1u)
{
    AV_SCOPE("DxgiFormatLayout::DxgiFormatLayout");
    if (format == a4b4g4r4) {
        bytes_ = 2u;
        return;
    }
    if (format > b4g4r4a4) return;
    bytes_ = element_sizes[format];
    if (format == r8g8_b8g8 || format == g8r8_g8b8 || format == yuy2 || format == y210 || format == y216) {
        width_ = 2u;
    }
    if ((format >= first_bc && format <= last_bc1_3) || (format >= first_bc6 && format <= last_bc)) {
        width_ = block;
        height_ = block;
    }
}

uint32_t DxgiFormatLayout::element_bytes() const
{
    AV_SCOPE("DxgiFormatLayout::element_bytes");
    return bytes_;
}

uint32_t DxgiFormatLayout::element_width() const
{
    AV_SCOPE("DxgiFormatLayout::element_width");
    return width_;
}

uint32_t DxgiFormatLayout::element_height() const
{
    AV_SCOPE("DxgiFormatLayout::element_height");
    return height_;
}

uint32_t DxgiFormatLayout::row_bytes(uint32_t width) const
{
    AV_SCOPE("DxgiFormatLayout::row_bytes");
    return (width + width_ - 1u) / width_ * bytes_;
}

uint32_t DxgiFormatLayout::rows(uint32_t height) const
{
    AV_SCOPE("DxgiFormatLayout::rows");
    return (height + height_ - 1u) / height_;
}

}  // namespace av
