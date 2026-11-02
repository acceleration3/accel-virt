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

}

TEST(dxgi_format_layout, gives_each_format_its_element)
{ const Expected formats[] = {
      {0u, 0u, 1u, 1u},
      {2u, 16u, 1u, 1u},
      {6u, 12u, 1u, 1u},
      {10u, 8u, 1u, 1u},
      {22u, 8u, 1u, 1u},
      {23u, 4u, 1u, 1u},
      {28u, 4u, 1u, 1u},
      {47u, 4u, 1u, 1u},
      {48u, 2u, 1u, 1u},
      {54u, 2u, 1u, 1u},
      {61u, 1u, 1u, 1u},
      {65u, 1u, 1u, 1u},
      {66u, 0u, 1u, 1u},
      {67u, 4u, 1u, 1u},
      {68u, 4u, 2u, 1u},
      {69u, 4u, 2u, 1u},
      {70u, 8u, 4u, 4u},
      {71u, 8u, 4u, 4u},
      {77u, 16u, 4u, 4u},
      {80u, 8u, 4u, 4u},
      {84u, 16u, 4u, 4u},
      {85u, 2u, 1u, 1u},
      {87u, 4u, 1u, 1u},
      {93u, 4u, 1u, 1u},
      {94u, 16u, 4u, 4u},
      {98u, 16u, 4u, 4u},
      {99u, 16u, 4u, 4u},
      {100u, 4u, 1u, 1u},
      {103u, 0u, 1u, 1u},
      {107u, 4u, 2u, 1u},
      {108u, 8u, 2u, 1u},
      {109u, 8u, 2u, 1u},
      {110u, 0u, 1u, 1u},
      {113u, 1u, 1u, 1u},
      {115u, 2u, 1u, 1u},
      {116u, 0u, 1u, 1u},
      {190u, 0u, 1u, 1u},
      {191u, 2u, 1u, 1u},
      {192u, 0u, 1u, 1u},
      {0xFFFFFFFFu, 0u, 1u, 1u},
  };
  for (const Expected &one : formats) {
      const DxgiFormatLayout layout(one.format);
      CHECK_EQ(layout.element_bytes(), one.bytes);
      CHECK_EQ(layout.element_width(), one.width);
      CHECK_EQ(layout.element_height(), one.height);
  } }

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

TEST(dxgi_format_layout, knows_every_value_dxgi_has_and_none_past_it)
{ CHECK(DxgiFormatLayout(0u).known());
  CHECK(DxgiFormatLayout(28u).known());
  CHECK(DxgiFormatLayout(130u).known());
  CHECK_EQ(DxgiFormatLayout(130u).element_bytes(), 0u);
  CHECK(DxgiFormatLayout(191u).known());
  CHECK(!DxgiFormatLayout(192u).known());
  CHECK(!DxgiFormatLayout(0xFFFFFFFFu).known()); }
