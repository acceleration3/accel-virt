#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

#include <cstdint>

using av::D3d9FormatLayout;

namespace {

struct Expected {
    std::uint32_t format;
    std::uint32_t bytes;
    std::uint32_t width;
    std::uint32_t height;
};

constexpr std::uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8u |
           static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16u |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24u;
}

}  // namespace

/* Every run of D3DFORMAT values and both edges of it, and each FourCC Direct3D 9
 * applications upload: a format given the wrong element writes a texture's
 * rows a whole element apart. Values are d3d9types.h's. */
TEST(d3d9_format_layout, gives_each_format_its_element)
{
    const Expected formats[] = {
        {0u, 0u, 1u, 1u},                           /* UNKNOWN */
        {19u, 0u, 1u, 1u},                          /* just before R8G8B8 */
        {20u, 3u, 1u, 1u},                          /* R8G8B8 */
        {21u, 4u, 1u, 1u},                          /* A8R8G8B8 */
        {22u, 4u, 1u, 1u},                          /* X8R8G8B8 */
        {23u, 2u, 1u, 1u},                          /* R5G6B5 */
        {26u, 2u, 1u, 1u},                          /* A4R4G4B4 */
        {27u, 1u, 1u, 1u},                          /* R3G3B2 */
        {28u, 1u, 1u, 1u},                          /* A8 */
        {29u, 2u, 1u, 1u},                          /* A8R3G3B2 */
        {30u, 2u, 1u, 1u},                          /* X4R4G4B4 */
        {31u, 4u, 1u, 1u},                          /* A2B10G10R10 */
        {34u, 4u, 1u, 1u},                          /* G16R16 */
        {35u, 4u, 1u, 1u},                          /* A2R10G10B10 */
        {36u, 8u, 1u, 1u},                          /* A16B16G16R16 */
        {37u, 0u, 1u, 1u},                          /* a gap */
        {40u, 2u, 1u, 1u},                          /* A8P8 */
        {41u, 1u, 1u, 1u},                          /* P8 */
        {50u, 1u, 1u, 1u},                          /* L8 */
        {51u, 2u, 1u, 1u},                          /* A8L8 */
        {52u, 1u, 1u, 1u},                          /* A4L4 */
        {60u, 2u, 1u, 1u},                          /* V8U8 */
        {62u, 4u, 1u, 1u},                          /* X8L8V8U8 */
        {64u, 4u, 1u, 1u},                          /* V16U16 */
        {67u, 4u, 1u, 1u},                          /* A2W10V10U10 */
        {70u, 2u, 1u, 1u},                          /* D16_LOCKABLE */
        {71u, 4u, 1u, 1u},                          /* D32 */
        {73u, 2u, 1u, 1u},                          /* D15S1 */
        {75u, 4u, 1u, 1u},                          /* D24S8 */
        {80u, 2u, 1u, 1u},                          /* D16 */
        {81u, 2u, 1u, 1u},                          /* L16 */
        {82u, 4u, 1u, 1u},                          /* D32F_LOCKABLE */
        {83u, 4u, 1u, 1u},                          /* D24FS8 */
        {84u, 4u, 1u, 1u},                          /* D32_LOCKABLE */
        {85u, 1u, 1u, 1u},                          /* S8_LOCKABLE */
        {101u, 2u, 1u, 1u},                         /* INDEX16 */
        {102u, 4u, 1u, 1u},                         /* INDEX32 */
        {110u, 8u, 1u, 1u},                         /* Q16W16V16U16 */
        {111u, 2u, 1u, 1u},                         /* R16F */
        {112u, 4u, 1u, 1u},                         /* G16R16F */
        {113u, 8u, 1u, 1u},                         /* A16B16G16R16F */
        {114u, 4u, 1u, 1u},                         /* R32F */
        {115u, 8u, 1u, 1u},                         /* G32R32F */
        {116u, 16u, 1u, 1u},                        /* A32B32G32R32F */
        {117u, 2u, 1u, 1u},                         /* CxV8U8 */
        {118u, 0u, 1u, 1u},                         /* A1: no whole byte per texel */
        {119u, 4u, 1u, 1u},                         /* A2B10G10R10_XR_BIAS */
        {120u, 0u, 1u, 1u},                         /* past the last */
        {fourcc('D', 'X', 'T', '1'), 8u, 4u, 4u},
        {fourcc('D', 'X', 'T', '2'), 16u, 4u, 4u},
        {fourcc('D', 'X', 'T', '3'), 16u, 4u, 4u},
        {fourcc('D', 'X', 'T', '4'), 16u, 4u, 4u},
        {fourcc('D', 'X', 'T', '5'), 16u, 4u, 4u},
        {fourcc('A', 'T', 'I', '1'), 8u, 4u, 4u},
        {fourcc('A', 'T', 'I', '2'), 16u, 4u, 4u},
        {fourcc('U', 'Y', 'V', 'Y'), 4u, 2u, 1u},
        {fourcc('Y', 'U', 'Y', '2'), 4u, 2u, 1u},
        {fourcc('R', 'G', 'B', 'G'), 4u, 2u, 1u},    /* R8G8_B8G8 */
        {fourcc('G', 'R', 'G', 'B'), 4u, 2u, 1u},    /* G8R8_G8B8 */
        {fourcc('I', 'N', 'T', 'Z'), 4u, 1u, 1u},
        {fourcc('D', 'F', '2', '4'), 4u, 1u, 1u},
        {fourcc('D', 'F', '1', '6'), 2u, 1u, 1u},
        {fourcc('R', 'A', 'W', 'Z'), 4u, 1u, 1u},
        {fourcc('N', 'U', 'L', 'L'), 0u, 1u, 1u},    /* a render target with no memory */
        {fourcc('D', 'X', 'T', '6'), 0u, 1u, 1u},    /* no such format */
    };
    for (const Expected &expected : formats) {
        const D3d9FormatLayout layout(expected.format);
        CHECK_EQ(layout.element_bytes(), expected.bytes);
        CHECK_EQ(layout.element_width(), expected.width);
        CHECK_EQ(layout.element_height(), expected.height);
    }
}

/* A row and a column of a block format count whole blocks, so a 1x1 mip of a
 * DXT1 texture is one block of 8 bytes, not an eighth of one. */
TEST(d3d9_format_layout, counts_rows_and_bytes_in_whole_elements)
{
    const D3d9FormatLayout dxt1(fourcc('D', 'X', 'T', '1'));
    CHECK_EQ(dxt1.row_bytes(1u), 8u);
    CHECK_EQ(dxt1.row_bytes(4u), 8u);
    CHECK_EQ(dxt1.row_bytes(5u), 16u);
    CHECK_EQ(dxt1.rows(1u), 1u);
    CHECK_EQ(dxt1.rows(4u), 1u);
    CHECK_EQ(dxt1.rows(5u), 2u);
    const D3d9FormatLayout yuy2(fourcc('Y', 'U', 'Y', '2'));
    CHECK_EQ(yuy2.row_bytes(3u), 8u);
    CHECK_EQ(yuy2.rows(3u), 3u);
    const D3d9FormatLayout argb(21u);
    CHECK_EQ(argb.row_bytes(7u), 28u);
    CHECK_EQ(argb.rows(7u), 7u);
    const D3d9FormatLayout unknown(0u);
    CHECK_EQ(unknown.row_bytes(64u), 0u);
}
