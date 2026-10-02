#include "harness/framework/test_registry.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"

#include <cstdint>

using av::DxgiFormatLayout;

namespace {

struct Expected {
    std::uint32_t format;
    std::uint32_t bytes;
    std::uint32_t width;
    std::uint32_t height;
};

}  // namespace

/* One format from each run of the table, and each edge of a run: an entry off
 * by one shifts every format behind it, and a readback packs rows a whole
 * element wrong. Values are DXGI_FORMAT's (dxgiformat.h). */
TEST(dxgi_format_layout, gives_each_format_its_element)
{ const Expected formats[] = {
      {0u, 0u, 1u, 1u},      /* UNKNOWN */
      {2u, 16u, 1u, 1u},     /* R32G32B32A32_FLOAT */
      {6u, 12u, 1u, 1u},     /* R32G32B32_FLOAT */
      {10u, 8u, 1u, 1u},     /* R16G16B16A16_FLOAT */
      {22u, 8u, 1u, 1u},     /* X32_TYPELESS_G8X24_UINT */
      {23u, 4u, 1u, 1u},     /* R10G10B10A2_TYPELESS */
      {28u, 4u, 1u, 1u},     /* R8G8B8A8_UNORM */
      {47u, 4u, 1u, 1u},     /* X24_TYPELESS_G8_UINT */
      {48u, 2u, 1u, 1u},     /* R8G8_TYPELESS */
      {54u, 2u, 1u, 1u},     /* R16_FLOAT */
      {61u, 1u, 1u, 1u},     /* R8_UNORM */
      {65u, 1u, 1u, 1u},     /* A8_UNORM */
      {66u, 0u, 1u, 1u},     /* R1_UNORM: no whole byte per element */
      {67u, 4u, 1u, 1u},     /* R9G9B9E5_SHAREDEXP */
      {68u, 4u, 2u, 1u},     /* R8G8_B8G8_UNORM: two texels per element */
      {69u, 4u, 2u, 1u},     /* G8R8_G8B8_UNORM */
      {70u, 8u, 4u, 4u},     /* BC1_TYPELESS */
      {71u, 8u, 4u, 4u},     /* BC1_UNORM */
      {77u, 16u, 4u, 4u},    /* BC3_UNORM */
      {80u, 8u, 4u, 4u},     /* BC4_UNORM */
      {84u, 16u, 4u, 4u},    /* BC5_SNORM */
      {85u, 2u, 1u, 1u},     /* B5G6R5_UNORM, just past the first blocks */
      {87u, 4u, 1u, 1u},     /* B8G8R8A8_UNORM */
      {93u, 4u, 1u, 1u},     /* B8G8R8X8_UNORM_SRGB, just before BC6H */
      {94u, 16u, 4u, 4u},    /* BC6H_TYPELESS */
      {98u, 16u, 4u, 4u},    /* BC7_UNORM */
      {99u, 16u, 4u, 4u},    /* BC7_UNORM_SRGB */
      {100u, 4u, 1u, 1u},    /* AYUV, just past the last block */
      {103u, 0u, 1u, 1u},    /* NV12: planar */
      {107u, 4u, 2u, 1u},    /* YUY2 */
      {108u, 8u, 2u, 1u},    /* Y210 */
      {109u, 8u, 2u, 1u},    /* Y216 */
      {110u, 0u, 1u, 1u},    /* NV11: planar */
      {113u, 1u, 1u, 1u},    /* P8 */
      {115u, 2u, 1u, 1u},    /* B4G4R4A4_UNORM, the table's last */
      {116u, 0u, 1u, 1u},    /* past the table */
      {190u, 0u, 1u, 1u},
      {191u, 2u, 1u, 1u},    /* A4B4G4R4_UNORM, outside it */
      {192u, 0u, 1u, 1u},
      {0xFFFFFFFFu, 0u, 1u, 1u},
  };
  for (const Expected &one : formats) {
      const DxgiFormatLayout layout(one.format);
      CHECK_EQ(layout.element_bytes(), one.bytes);
      CHECK_EQ(layout.element_width(), one.width);
      CHECK_EQ(layout.element_height(), one.height);
  } }

/* A row is whole elements: a 10-texel row of BC1 is three blocks, of YUY2 five
 * texels are three pairs, and a partial block row is a whole one. */
TEST(dxgi_format_layout, rounds_rows_and_their_bytes_up_to_whole_elements)
{ const DxgiFormatLayout bc1(71u);
  CHECK_EQ(bc1.row_bytes(10u), 24u);
  CHECK_EQ(bc1.row_bytes(8u), 16u);
  CHECK_EQ(bc1.row_bytes(1u), 8u);
  CHECK_EQ(bc1.rows(10u), 3u);
  CHECK_EQ(bc1.rows(8u), 2u);
  CHECK_EQ(bc1.rows(0u), 0u);
  const DxgiFormatLayout yuy2(107u);
  CHECK_EQ(yuy2.row_bytes(5u), 12u);
  CHECK_EQ(yuy2.row_bytes(4u), 8u);
  CHECK_EQ(yuy2.rows(5u), 5u);
  const DxgiFormatLayout rgba(28u);
  CHECK_EQ(rgba.row_bytes(1280u), 5120u);
  CHECK_EQ(rgba.rows(800u), 800u);
  const DxgiFormatLayout unknown(0u);
  CHECK_EQ(unknown.row_bytes(64u), 0u); }

/* DXGI_FORMAT's values end at A4B4G4R4_UNORM (191): a guest naming one past
 * it names no format, while a typeless or planar one is known and has no
 * layout. */
TEST(dxgi_format_layout, knows_every_value_dxgi_has_and_none_past_it)
{ CHECK(DxgiFormatLayout(0u).known());
  CHECK(DxgiFormatLayout(28u).known());
  CHECK(DxgiFormatLayout(130u).known());     /* a planar format, past the table */
  CHECK_EQ(DxgiFormatLayout(130u).element_bytes(), 0u);
  CHECK(DxgiFormatLayout(191u).known());
  CHECK(!DxgiFormatLayout(192u).known());
  CHECK(!DxgiFormatLayout(0xFFFFFFFFu).known()); }
