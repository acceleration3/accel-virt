#include "core/support/four_cc.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

#include <cstdint>

using av::D3d9FormatLayout;
using av::FourCc;

namespace {

struct Expected {
    std::uint32_t format;
    std::uint32_t bytes;
    std::uint32_t width;
    std::uint32_t height;
};

}

TEST(d3d9_format_layout, gives_each_format_its_element)
{
    const Expected formats[] = {
        {0u, 0u, 1u, 1u},
        {19u, 0u, 1u, 1u},
        {20u, 3u, 1u, 1u},
        {21u, 4u, 1u, 1u},
        {22u, 4u, 1u, 1u},
        {23u, 2u, 1u, 1u},
        {26u, 2u, 1u, 1u},
        {27u, 1u, 1u, 1u},
        {28u, 1u, 1u, 1u},
        {29u, 2u, 1u, 1u},
        {30u, 2u, 1u, 1u},
        {31u, 4u, 1u, 1u},
        {34u, 4u, 1u, 1u},
        {35u, 4u, 1u, 1u},
        {36u, 8u, 1u, 1u},
        {37u, 0u, 1u, 1u},
        {40u, 2u, 1u, 1u},
        {41u, 1u, 1u, 1u},
        {50u, 1u, 1u, 1u},
        {51u, 2u, 1u, 1u},
        {52u, 1u, 1u, 1u},
        {60u, 2u, 1u, 1u},
        {62u, 4u, 1u, 1u},
        {64u, 4u, 1u, 1u},
        {67u, 4u, 1u, 1u},
        {70u, 2u, 1u, 1u},
        {71u, 4u, 1u, 1u},
        {73u, 2u, 1u, 1u},
        {75u, 4u, 1u, 1u},
        {80u, 2u, 1u, 1u},
        {81u, 2u, 1u, 1u},
        {82u, 4u, 1u, 1u},
        {83u, 4u, 1u, 1u},
        {84u, 4u, 1u, 1u},
        {85u, 1u, 1u, 1u},
        {101u, 2u, 1u, 1u},
        {102u, 4u, 1u, 1u},
        {110u, 8u, 1u, 1u},
        {111u, 2u, 1u, 1u},
        {112u, 4u, 1u, 1u},
        {113u, 8u, 1u, 1u},
        {114u, 4u, 1u, 1u},
        {115u, 8u, 1u, 1u},
        {116u, 16u, 1u, 1u},
        {117u, 2u, 1u, 1u},
        {118u, 0u, 1u, 1u},
        {119u, 4u, 1u, 1u},
        {120u, 0u, 1u, 1u},
        {FourCc('D', 'X', 'T', '1').value(), 8u, 4u, 4u},
        {FourCc('D', 'X', 'T', '2').value(), 16u, 4u, 4u},
        {FourCc('D', 'X', 'T', '3').value(), 16u, 4u, 4u},
        {FourCc('D', 'X', 'T', '4').value(), 16u, 4u, 4u},
        {FourCc('D', 'X', 'T', '5').value(), 16u, 4u, 4u},
        {FourCc('A', 'T', 'I', '1').value(), 8u, 4u, 4u},
        {FourCc('A', 'T', 'I', '2').value(), 16u, 4u, 4u},
        {FourCc('U', 'Y', 'V', 'Y').value(), 4u, 2u, 1u},
        {FourCc('Y', 'U', 'Y', '2').value(), 4u, 2u, 1u},
        {FourCc('R', 'G', 'B', 'G').value(), 4u, 2u, 1u},
        {FourCc('G', 'R', 'G', 'B').value(), 4u, 2u, 1u},
        {FourCc('I', 'N', 'T', 'Z').value(), 4u, 1u, 1u},
        {FourCc('D', 'F', '2', '4').value(), 4u, 1u, 1u},
        {FourCc('D', 'F', '1', '6').value(), 2u, 1u, 1u},
        {FourCc('R', 'A', 'W', 'Z').value(), 4u, 1u, 1u},
        {FourCc('N', 'U', 'L', 'L').value(), 0u, 1u, 1u},
        {FourCc('D', 'X', 'T', '6').value(), 0u, 1u, 1u},
    };
    for (const Expected &expected : formats) {
        const D3d9FormatLayout layout(expected.format);
        CHECK_EQ(layout.element_bytes(), expected.bytes);
        CHECK_EQ(layout.element_width(), expected.width);
        CHECK_EQ(layout.element_height(), expected.height);
    }
}

TEST(d3d9_format_layout, counts_rows_and_bytes_in_whole_elements)
{
    const D3d9FormatLayout dxt1(FourCc('D', 'X', 'T', '1').value());
    CHECK_EQ(dxt1.row_bytes(1u), 8u);
    CHECK_EQ(dxt1.row_bytes(4u), 8u);
    CHECK_EQ(dxt1.row_bytes(5u), 16u);
    CHECK_EQ(dxt1.rows(1u), 1u);
    CHECK_EQ(dxt1.rows(4u), 1u);
    CHECK_EQ(dxt1.rows(5u), 2u);
    const D3d9FormatLayout yuy2(FourCc('Y', 'U', 'Y', '2').value());
    CHECK_EQ(yuy2.row_bytes(3u), 8u);
    CHECK_EQ(yuy2.rows(3u), 3u);
    const D3d9FormatLayout argb(21u);
    CHECK_EQ(argb.row_bytes(7u), 28u);
    CHECK_EQ(argb.rows(7u), 7u);
    const D3d9FormatLayout unknown(0u);
    CHECK_EQ(unknown.row_bytes(64u), 0u);
}
