#include "proto/d3d/d3d9_format_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* Bytes per texel of every D3DFORMAT from R8G8B8 (20) to A2B10G10R10_XR_BIAS
 * (119), in order; 0 for the values d3d9types.h leaves unnamed and for A1. */
constexpr uint8_t texel_sizes[] = {
    3, 4, 4, 2, 2, 2, 2, 1, 1, 2, 2, 4, 4, 4, 4, 4, 8, 0, 0, 0,     /* 20-39 */
    2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0,     /* 40-59 */
    2, 2, 4, 4, 4, 0, 0, 4, 0, 0, 2, 4, 0, 2, 0, 4, 0, 4, 0, 4,     /* 60-79 */
    2, 2, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 80-99 */
    0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 8, 2, 4, 8, 4, 8, 16, 2, 0, 4};  /* 100-119 */

constexpr uint32_t first_texel_format = 20u;   /* D3DFMT_R8G8B8 */
constexpr uint32_t last_texel_format = 119u;   /* D3DFMT_A2B10G10R10_XR_BIAS */
constexpr uint32_t block = 4u;
constexpr uint32_t pair = 2u;

constexpr uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<uint32_t>(static_cast<unsigned char>(a)) |
           static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8u |
           static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16u |
           static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24u;
}

}  // namespace

D3d9FormatLayout::D3d9FormatLayout(uint32_t format) : bytes_(0u), width_(1u), height_(1u)
{
    AV_SCOPE("D3d9FormatLayout::D3d9FormatLayout");
    if (format >= first_texel_format && format <= last_texel_format) {
        bytes_ = texel_sizes[format - first_texel_format];
        return;
    }
    four_cc(format);
}

void D3d9FormatLayout::four_cc(uint32_t format)
{
    AV_SCOPE("D3d9FormatLayout::four_cc");
    switch (format) {
    case fourcc('D', 'X', 'T', '1'):
    case fourcc('A', 'T', 'I', '1'):
        bytes_ = 8u;
        width_ = block;
        height_ = block;
        return;
    case fourcc('D', 'X', 'T', '2'):
    case fourcc('D', 'X', 'T', '3'):
    case fourcc('D', 'X', 'T', '4'):
    case fourcc('D', 'X', 'T', '5'):
    case fourcc('A', 'T', 'I', '2'):
        bytes_ = 16u;
        width_ = block;
        height_ = block;
        return;
    case fourcc('U', 'Y', 'V', 'Y'):
    case fourcc('Y', 'U', 'Y', '2'):
    case fourcc('R', 'G', 'B', 'G'):
    case fourcc('G', 'R', 'G', 'B'):
        bytes_ = 4u;
        width_ = pair;
        return;
    case fourcc('I', 'N', 'T', 'Z'):
    case fourcc('D', 'F', '2', '4'):
    case fourcc('R', 'A', 'W', 'Z'): bytes_ = 4u; return;
    case fourcc('D', 'F', '1', '6'): bytes_ = 2u; return;
    default: return;
    }
}

uint32_t D3d9FormatLayout::element_bytes() const
{
    AV_SCOPE("D3d9FormatLayout::element_bytes");
    return bytes_;
}

uint32_t D3d9FormatLayout::element_width() const
{
    AV_SCOPE("D3d9FormatLayout::element_width");
    return width_;
}

uint32_t D3d9FormatLayout::element_height() const
{
    AV_SCOPE("D3d9FormatLayout::element_height");
    return height_;
}

uint32_t D3d9FormatLayout::row_bytes(uint32_t width) const
{
    AV_SCOPE("D3d9FormatLayout::row_bytes");
    return (width + width_ - 1u) / width_ * bytes_;
}

uint32_t D3d9FormatLayout::rows(uint32_t height) const
{
    AV_SCOPE("D3d9FormatLayout::rows");
    return (height + height_ - 1u) / height_;
}

}  // namespace av
