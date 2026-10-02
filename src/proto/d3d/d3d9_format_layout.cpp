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

D3d9FormatLayout::D3d9FormatLayout(uint32_t format) : format_(format)
{
    AV_SCOPE("D3d9FormatLayout::D3d9FormatLayout");
    if (format_ >= first_texel_format && format_ <= last_texel_format) {
        lay_out(texel_sizes[format_ - first_texel_format], 1u, 1u);
        return;
    }
    four_cc();
}

void D3d9FormatLayout::four_cc()
{
    AV_SCOPE("D3d9FormatLayout::four_cc");
    switch (format_) {
    case fourcc('D', 'X', 'T', '1'):
    case fourcc('A', 'T', 'I', '1'): lay_out(8u, block, block); return;
    case fourcc('D', 'X', 'T', '2'):
    case fourcc('D', 'X', 'T', '3'):
    case fourcc('D', 'X', 'T', '4'):
    case fourcc('D', 'X', 'T', '5'):
    case fourcc('A', 'T', 'I', '2'): lay_out(16u, block, block); return;
    case fourcc('U', 'Y', 'V', 'Y'):
    case fourcc('Y', 'U', 'Y', '2'):
    case fourcc('R', 'G', 'B', 'G'):
    case fourcc('G', 'R', 'G', 'B'): lay_out(4u, pair, 1u); return;
    case fourcc('I', 'N', 'T', 'Z'):
    case fourcc('D', 'F', '2', '4'):
    case fourcc('R', 'A', 'W', 'Z'): lay_out(4u, 1u, 1u); return;
    case fourcc('D', 'F', '1', '6'): lay_out(2u, 1u, 1u); return;
    default: return;
    }
}

}  // namespace av
