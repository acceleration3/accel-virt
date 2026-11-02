#include "proto/d3d/d3d9_format_layout.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint8_t texel_sizes[] = {
    3, 4, 4, 2, 2, 2, 2, 1, 1, 2, 2, 4, 4, 4, 4, 4, 8, 0, 0, 0,
    2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 4, 4, 4, 0, 0, 4, 0, 0, 2, 4, 0, 2, 0, 4, 0, 4, 0, 4,
    2, 2, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 8, 2, 4, 8, 4, 8, 16, 2, 0, 4};

constexpr uint32_t first_texel_format = 20u;
constexpr uint32_t last_texel_format = 119u;
constexpr uint32_t block = 4u;
constexpr uint32_t pair = 2u;

}

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
    case FourCc('D', 'X', 'T', '1').value():
    case FourCc('A', 'T', 'I', '1').value(): lay_out(8u, block, block); return;
    case FourCc('D', 'X', 'T', '2').value():
    case FourCc('D', 'X', 'T', '3').value():
    case FourCc('D', 'X', 'T', '4').value():
    case FourCc('D', 'X', 'T', '5').value():
    case FourCc('A', 'T', 'I', '2').value(): lay_out(16u, block, block); return;
    case FourCc('U', 'Y', 'V', 'Y').value():
    case FourCc('Y', 'U', 'Y', '2').value():
    case FourCc('R', 'G', 'B', 'G').value():
    case FourCc('G', 'R', 'G', 'B').value(): lay_out(4u, pair, 1u); return;
    case FourCc('I', 'N', 'T', 'Z').value():
    case FourCc('D', 'F', '2', '4').value():
    case FourCc('R', 'A', 'W', 'Z').value(): lay_out(4u, 1u, 1u); return;
    case FourCc('D', 'F', '1', '6').value(): lay_out(2u, 1u, 1u); return;
    default: return;
    }
}

}
