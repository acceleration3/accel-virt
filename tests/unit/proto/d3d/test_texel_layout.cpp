/* What every runtime's format layout shares (TexelLayout), through one of
 * each: rows and their bytes in whole elements, a format with no layout copied
 * a byte a texel and counted a row a texel, and which layouts are a four-byte
 * element a texel - the only ones a presented frame is staged as. Values are
 * DXGI_FORMAT's and D3DFORMAT's. */
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/d3d/texel_layout.hpp"

#include <cstdint>

using av::D3d9FormatLayout;
using av::DxgiFormatLayout;
using av::TexelLayout;

namespace {

constexpr std::uint32_t dxgi_unknown = 0u;
constexpr std::uint32_t dxgi_rgba8 = 28u;
constexpr std::uint32_t dxgi_bgra8 = 87u;
constexpr std::uint32_t dxgi_r16g16 = 35u;
constexpr std::uint32_t dxgi_bc1 = 71u;
constexpr std::uint32_t dxgi_yuy2 = 107u;
constexpr std::uint32_t d3d9_a8r8g8b8 = 21u;
constexpr std::uint32_t d3d9_r5g6b5 = 23u;
constexpr std::uint32_t d3d9_a1 = 118u;

std::uint32_t copied(const TexelLayout &layout, std::uint32_t width) { return layout.copied_row_bytes(width); }

}  // namespace

TEST(texel_layout, a_format_with_no_layout_is_a_row_a_texel_of_no_bytes)
{ const DxgiFormatLayout unknown(dxgi_unknown);
  CHECK_EQ(unknown.element_bytes(), 0u);
  CHECK_EQ(unknown.element_width(), 1u);
  CHECK_EQ(unknown.element_height(), 1u);
  CHECK_EQ(unknown.rows(37u), 37u);
  CHECK_EQ(unknown.row_bytes(37u), 0u);
  const D3d9FormatLayout a1(d3d9_a1);
  CHECK_EQ(a1.rows(5u), 5u);
  CHECK_EQ(a1.row_bytes(5u), 0u); }

/* A copy moves what the layout says, and for a format with none a byte a
 * texel: rows of an unknown format still reach the other end. */
TEST(texel_layout, a_copied_row_is_its_bytes_or_a_byte_a_texel)
{ CHECK_EQ(copied(DxgiFormatLayout(dxgi_unknown), 37u), 37u);
  CHECK_EQ(copied(D3d9FormatLayout(d3d9_a1), 9u), 9u);
  CHECK_EQ(copied(DxgiFormatLayout(dxgi_rgba8), 37u), 148u);
  CHECK_EQ(copied(DxgiFormatLayout(dxgi_bc1), 10u), 24u);
  CHECK_EQ(copied(D3d9FormatLayout(d3d9_r5g6b5), 3u), 6u); }

/* Four bytes, one texel wide and one high - not four bytes of a 4:2:2 pair,
 * nor a block, nor a two-byte texel. */
TEST(texel_layout, only_a_four_byte_element_a_texel_is_word_texels)
{ CHECK(DxgiFormatLayout(dxgi_rgba8).word_texels());
  CHECK(DxgiFormatLayout(dxgi_bgra8).word_texels());
  CHECK(DxgiFormatLayout(dxgi_r16g16).word_texels());
  CHECK(D3d9FormatLayout(d3d9_a8r8g8b8).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_yuy2).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_bc1).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_unknown).word_texels());
  CHECK(!D3d9FormatLayout(d3d9_r5g6b5).word_texels()); }

/* A copy of a layout is the same layout. */
TEST(texel_layout, a_copied_layout_counts_the_same)
{ const DxgiFormatLayout bc1(dxgi_bc1);
  const DxgiFormatLayout again = bc1;
  CHECK_EQ(again.rows(10u), 3u);
  CHECK_EQ(again.row_bytes(10u), 24u);
  CHECK_EQ(again.element_width(), 4u); }
