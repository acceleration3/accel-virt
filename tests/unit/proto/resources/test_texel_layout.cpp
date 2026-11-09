#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/resources/texel_layout.hpp"

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

}

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

TEST(texel_layout, a_copied_row_is_its_bytes_or_a_byte_a_texel)
{ CHECK_EQ(copied(DxgiFormatLayout(dxgi_unknown), 37u), 37u);
  CHECK_EQ(copied(D3d9FormatLayout(d3d9_a1), 9u), 9u);
  CHECK_EQ(copied(DxgiFormatLayout(dxgi_rgba8), 37u), 148u);
  CHECK_EQ(copied(DxgiFormatLayout(dxgi_bc1), 10u), 24u);
  CHECK_EQ(copied(D3d9FormatLayout(d3d9_r5g6b5), 3u), 6u); }

TEST(texel_layout, only_a_four_byte_element_a_texel_is_word_texels)
{ CHECK(DxgiFormatLayout(dxgi_rgba8).word_texels());
  CHECK(DxgiFormatLayout(dxgi_bgra8).word_texels());
  CHECK(DxgiFormatLayout(dxgi_r16g16).word_texels());
  CHECK(D3d9FormatLayout(d3d9_a8r8g8b8).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_yuy2).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_bc1).word_texels());
  CHECK(!DxgiFormatLayout(dxgi_unknown).word_texels());
  CHECK(!D3d9FormatLayout(d3d9_r5g6b5).word_texels()); }

TEST(texel_layout, a_copied_layout_counts_the_same)
{ const DxgiFormatLayout bc1(dxgi_bc1);
  const DxgiFormatLayout again = bc1;
  CHECK_EQ(again.rows(10u), 3u);
  CHECK_EQ(again.row_bytes(10u), 24u);
  CHECK_EQ(again.element_width(), 4u); }

TEST(texel_layout, an_element_of_no_texels_is_one)
{ const TexelLayout flat(4u, 0u, 0u);
  CHECK_EQ(flat.element_width(), 1u);
  CHECK_EQ(flat.element_height(), 1u);
  CHECK_EQ(flat.row_bytes(3u), 12u);
  CHECK_EQ(flat.rows(5u), 5u);
  const TexelLayout block(8u, 4u, 4u);
  CHECK_EQ(block.element_width(), 4u);
  CHECK_EQ(block.rows(5u), 2u); }

TEST(texel_layout, a_texel_starts_where_its_element_does)
{ const TexelLayout block(8u, 4u, 4u);
  CHECK_EQ(block.offset_of(9u, 6u, 256u), 256u + 16u);
  CHECK_EQ(block.offset_of(8u, 4u, 256u), 256u + 16u);
  CHECK_EQ(block.offset_of(0u, 0u, 256u), 0u);
  const TexelLayout word(4u, 1u, 1u);
  CHECK_EQ(word.offset_of(9u, 6u, 256u), 6u * 256u + 36u);
  CHECK_EQ(word.offset_of(0u, 70000u, 70000u), 70000ull * 70000ull); }

TEST(texel_layout, a_planar_video_format_counts_its_chroma_rows)
{ const DxgiFormatLayout nv12(103u);
  CHECK(nv12.planar());
  CHECK_EQ(nv12.element_bytes(), 0u);
  CHECK_EQ(nv12.row_bytes(128u), 128u);
  CHECK_EQ(nv12.copied_row_bytes(128u), 128u);
  CHECK_EQ(nv12.rows(128u), 192u);
  CHECK_EQ(nv12.rows(5u), 8u);
  const DxgiFormatLayout p010(104u);
  CHECK_EQ(p010.row_bytes(128u), 256u);
  CHECK_EQ(p010.rows(128u), 192u);
  CHECK(!DxgiFormatLayout(dxgi_yuy2).planar());
  CHECK(!DxgiFormatLayout(dxgi_bc1).planar());
  const DxgiFormatLayout again = nv12;
  CHECK_EQ(again.rows(128u), 192u); }
