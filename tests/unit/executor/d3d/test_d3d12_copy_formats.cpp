#include "executor/d3d/d3d12_copy_formats.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12CopyFormats;
using av::d3d12proto::Box;

namespace {

constexpr std::uint32_t r32g32b32a32_float = 2u;
constexpr std::uint32_t r32g32_float = 16u;
constexpr std::uint32_t r16g16b16a16_unorm = 11u;
constexpr std::uint32_t d32_float = 40u;
constexpr std::uint32_t r32_float = 41u;
constexpr std::uint32_t r8_typeless = 60u;
constexpr std::uint32_t r8_unorm = 61u;
constexpr std::uint32_t r8_uint = 62u;
constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t r16_unorm = 56u;
constexpr std::uint32_t r32g8x24_typeless = 19u;
constexpr std::uint32_t d32_float_s8x24 = 20u;
constexpr std::uint32_t r32_float_x8x24 = 21u;
constexpr std::uint32_t x32_g8x24_uint = 22u;
constexpr std::uint32_t r24g8_typeless = 44u;
constexpr std::uint32_t d24_unorm_s8 = 45u;
constexpr std::uint32_t r24_unorm_x8 = 46u;
constexpr std::uint32_t x24_g8_uint = 47u;
constexpr std::uint32_t r8g8_unorm = 49u;
constexpr std::uint32_t r16g16_unorm = 35u;
constexpr std::uint32_t nv12 = 103u;
constexpr std::uint32_t p010 = 104u;
constexpr std::uint32_t bc1_unorm = 71u;
constexpr std::uint32_t bc2_unorm = 74u;
constexpr std::uint32_t bc3_unorm = 77u;
constexpr std::uint32_t bc4_unorm = 80u;
constexpr std::uint32_t bc5_unorm = 83u;
constexpr std::uint32_t bc6h_uf16 = 95u;
constexpr std::uint32_t bc7_unorm = 98u;
constexpr std::uint32_t unknown_format = 0xFFFFu;
constexpr std::uint32_t r8g8b8a8_typeless = 27u;
constexpr std::uint32_t r8g8b8a8_uint = 30u;
constexpr std::uint32_t r32_typeless = 39u;
constexpr std::uint32_t r16g16b16a16_float = 10u;
constexpr std::uint32_t r32g32_typeless = 15u;
constexpr std::uint32_t r32g32b32a32_typeless = 1u;
constexpr std::uint32_t y410 = 101u;
constexpr std::uint32_t y416 = 102u;

bool same(const Box &a, const Box &b)
{
    return a.left == b.left && a.top == b.top && a.front == b.front && a.right == b.right && a.bottom == b.bottom &&
           a.back == b.back;
}

}

TEST(d3d12_copy_formats, formats_of_one_typeless_group_copy_to_each_other_depth_and_colour_included)
{ CHECK(D3d12CopyFormats(r8g8b8a8_unorm, r8g8b8a8_uint).same_texels());
  CHECK(D3d12CopyFormats(r8g8b8a8_typeless, r8g8b8a8_unorm).same_texels());
  CHECK(D3d12CopyFormats(r32_float, d32_float).same_texels());
  CHECK(D3d12CopyFormats(d32_float, r32_typeless).same_texels());
  CHECK(D3d12CopyFormats(d32_float_s8x24, r32_float_x8x24).same_texels());
  CHECK(D3d12CopyFormats(r32_float, r32_float).same_texels());
  CHECK(D3d12CopyFormats(bc1_unorm, bc1_unorm).same_texels());
  CHECK(D3d12CopyFormats(nv12, nv12).same_texels()); }

TEST(d3d12_copy_formats, formats_of_one_size_in_another_typeless_group_do_not_copy_to_each_other)
{ CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, b8g8r8a8_unorm).same_texels());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r32_float).same_texels());
  CHECK(!D3d12CopyFormats(d32_float, r8g8b8a8_unorm).same_texels());
  CHECK(!D3d12CopyFormats(r32_float, r16g16_unorm).same_texels());
  CHECK(!D3d12CopyFormats(d24_unorm_s8, r32_float).same_texels());
  CHECK(!D3d12CopyFormats(r32g8x24_typeless, r32g32_float).same_texels());
  CHECK(!D3d12CopyFormats(r16g16b16a16_unorm, r32g32_float).same_texels()); }

TEST(d3d12_copy_formats, formats_of_another_texel_size_or_block_shape_do_not)
{ CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r32g32b32a32_float).same_texels());
  CHECK(!D3d12CopyFormats(r32_float, r16_unorm).same_texels());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, bc3_unorm).same_texels());
  CHECK(!D3d12CopyFormats(bc1_unorm, bc3_unorm).same_texels());
  CHECK(!D3d12CopyFormats(nv12, r8_unorm).same_texels()); }

TEST(d3d12_copy_formats, a_format_no_one_knows_copies_to_nothing)
{ CHECK(!D3d12CopyFormats(unknown_format, unknown_format).same_texels());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, unknown_format).same_texels());
  CHECK(!D3d12CopyFormats(unknown_format, r8g8b8a8_unorm).same_texels());
  CHECK(!D3d12CopyFormats(0u, 0u).scaled()); }

TEST(d3d12_copy_formats, a_compressed_format_copies_to_and_from_the_uncompressed_format_of_its_block_size)
{ const std::uint32_t eight[] = {bc1_unorm, bc4_unorm};
  const std::uint32_t sixteen[] = {bc2_unorm, bc3_unorm, bc5_unorm, bc6h_uf16, bc7_unorm};
  for (const std::uint32_t block : eight) {
      CHECK(D3d12CopyFormats(block, r32g32_float).same_texels());
      CHECK(D3d12CopyFormats(r16g16b16a16_unorm, block).same_texels());
      CHECK(D3d12CopyFormats(block, r32g32_float).scaled());
      CHECK(!D3d12CopyFormats(block, r32g32b32a32_float).same_texels());
      CHECK(!D3d12CopyFormats(block, r8g8b8a8_unorm).same_texels());
  }
  for (const std::uint32_t block : sixteen) {
      CHECK(D3d12CopyFormats(block, r32g32b32a32_float).same_texels());
      CHECK(D3d12CopyFormats(r32g32b32a32_float, block).same_texels());
      CHECK(!D3d12CopyFormats(block, r32g32_float).same_texels());
  }
  CHECK(!D3d12CopyFormats(d32_float_s8x24, bc1_unorm).same_texels());
  CHECK(!D3d12CopyFormats(bc1_unorm, d32_float_s8x24).same_texels()); }

TEST(d3d12_copy_formats, a_compressed_format_copies_only_to_the_uncompressed_groups_the_rules_name)
{ const std::uint32_t eight_byte_others[] = {r32g8x24_typeless, r32_float_x8x24, x32_g8x24_uint, y416};
  for (const std::uint32_t other : eight_byte_others) {
      CHECK(!D3d12CopyFormats(bc1_unorm, other).same_texels());
      CHECK(!D3d12CopyFormats(other, bc4_unorm).same_texels());
      CHECK(!D3d12CopyFormats(bc1_unorm, other).scaled());
  }
  CHECK(!D3d12CopyFormats(bc3_unorm, y410).same_texels());
  CHECK(!D3d12CopyFormats(bc7_unorm, r32g32_float).same_texels());
  CHECK(D3d12CopyFormats(bc4_unorm, r16g16b16a16_float).same_texels());
  CHECK(D3d12CopyFormats(bc1_unorm, r32g32_typeless).same_texels());
  CHECK(D3d12CopyFormats(bc6h_uf16, r32g32b32a32_typeless).same_texels()); }

TEST(d3d12_copy_formats, a_region_is_measured_in_the_destination_s_blocks_when_the_formats_differ_in_block_size)
{ const D3d12CopyFormats into_texels(r32g32_float, bc1_unorm);
  CHECK_EQ(into_texels.destination_extent(16u, true), 4u);
  CHECK_EQ(into_texels.destination_extent(5u, false), 2u);
  const D3d12CopyFormats into_blocks(bc1_unorm, r32g32_float);
  CHECK_EQ(into_blocks.destination_extent(4u, true), 16u);
  const D3d12CopyFormats alike(r8g8b8a8_unorm, b8g8r8a8_unorm);
  CHECK_EQ(alike.destination_extent(7u, true), 7u);
  CHECK(same(into_texels.rescaled(Box{3u, 2u, 0u, 19u, 10u, 1u}), Box{3u, 2u, 0u, 7u, 4u, 1u}));
  CHECK(same(into_blocks.rescaled(Box{1u, 1u, 0u, 3u, 5u, 1u}), Box{1u, 1u, 0u, 9u, 17u, 1u}));
  CHECK(same(alike.rescaled(Box{3u, 2u, 0u, 19u, 10u, 1u}), Box{3u, 2u, 0u, 19u, 10u, 1u}));
  CHECK(same(into_texels.rescaled(Box{8u, 0u, 0u, 4u, 4u, 1u}), Box{8u, 0u, 0u, 4u, 4u, 1u}));
  CHECK_EQ(into_blocks.destination_extent(0xFFFFFFFFu, true), 0xFFFFFFFFu); }

TEST(d3d12_copy_formats, a_depth_stencil_texture_is_laid_out_plane_by_plane_in_exactly_the_formats_that_name_the_plane)
{ CHECK(D3d12CopyFormats(d32_float_s8x24, r32_float).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(d32_float_s8x24, r32_float_x8x24).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(d32_float_s8x24, d32_float).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(d32_float_s8x24, r8_uint).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(d32_float_s8x24, x32_g8x24_uint).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(d32_float_s8x24, r8_uint).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(d32_float_s8x24, r32_float).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(d32_float_s8x24, r32g32b32a32_float).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(d32_float_s8x24, r32_float).footprint_of_plane(2u));
  CHECK(D3d12CopyFormats(r32g8x24_typeless, r32_typeless).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(r32g8x24_typeless, r8_typeless).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(d24_unorm_s8, r24_unorm_x8).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(d24_unorm_s8, x24_g8_uint).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(r24g8_typeless, r8_typeless).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(r24g8_typeless, r24_unorm_x8).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r24g8_typeless, r24_unorm_x8).footprint_of_plane(1u)); }

TEST(d3d12_copy_formats, a_planar_video_texture_is_laid_out_plane_by_plane)
{ CHECK(D3d12CopyFormats(nv12, r8_unorm).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(nv12, r8_typeless).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(nv12, r8g8_unorm).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(nv12, r8g8_unorm).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(nv12, r8_unorm).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(p010, r16_unorm).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(p010, r16g16_unorm).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(p010, r8g8_unorm).footprint_of_plane(1u)); }

TEST(d3d12_copy_formats, any_other_texture_has_one_plane_laid_out_in_a_format_of_its_typeless_group)
{ CHECK(D3d12CopyFormats(r8g8b8a8_unorm, r8g8b8a8_uint).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r8g8b8a8_uint).footprint_of_plane(1u));
  CHECK(D3d12CopyFormats(r8g8b8a8_unorm, r32_float).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(r8g8b8a8_unorm, b8g8r8a8_unorm).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(r32_float, r8g8b8a8_unorm).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r32_float).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(r32_float, d32_float).footprint_of_plane(1u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, d24_unorm_s8).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, d32_float).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(d32_float, r8g8b8a8_unorm).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(nv12, r8g8b8a8_unorm).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, nv12).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(bc1_unorm, bc3_unorm).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, unknown_format).footprint_of_plane(0u));
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r8_unorm).footprint_of_plane(0u));
  CHECK(D3d12CopyFormats(bc1_unorm, r32g32_float).footprint_of_plane(0u)); }
