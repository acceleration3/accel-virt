#include "executor/d3d/d3d12_copy_formats.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12CopyFormats;

namespace {

constexpr std::uint32_t r32g32b32a32_float = 2u;
constexpr std::uint32_t r32_float = 41u;
constexpr std::uint32_t r8_unorm = 61u;
constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t r16_unorm = 56u;
constexpr std::uint32_t d32_float_s8x24 = 20u;
constexpr std::uint32_t bc1_unorm = 71u;
constexpr std::uint32_t bc3_unorm = 77u;
constexpr std::uint32_t unknown_format = 0xFFFFu;

}

TEST(d3d12_copy_formats, formats_of_one_texel_size_and_block_shape_copy_to_each_other)
{ CHECK(D3d12CopyFormats(r8g8b8a8_unorm, b8g8r8a8_unorm).same_texels());
  CHECK(D3d12CopyFormats(r32_float, r32_float).same_texels());
  CHECK(D3d12CopyFormats(bc1_unorm, bc1_unorm).same_texels()); }

TEST(d3d12_copy_formats, formats_of_another_texel_size_or_block_shape_do_not)
{ CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r32g32b32a32_float).same_texels());
  CHECK(!D3d12CopyFormats(r32_float, r16_unorm).same_texels());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, bc3_unorm).same_texels());
  CHECK(!D3d12CopyFormats(bc1_unorm, bc3_unorm).same_texels()); }

TEST(d3d12_copy_formats, a_format_no_one_knows_copies_to_nothing)
{ CHECK(!D3d12CopyFormats(unknown_format, unknown_format).same_texels());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, unknown_format).same_texels());
  CHECK(!D3d12CopyFormats(unknown_format, r8g8b8a8_unorm).same_texels());
  CHECK(!D3d12CopyFormats(0u, 0u).same_texels()); }

TEST(d3d12_copy_formats, a_depth_stencil_texture_is_copied_a_plane_at_a_time)
{ CHECK(D3d12CopyFormats(d32_float_s8x24, r32_float).plane_of_destination());
  CHECK(D3d12CopyFormats(d32_float_s8x24, r8_unorm).plane_of_destination());
  CHECK(!D3d12CopyFormats(d32_float_s8x24, r32g32b32a32_float).plane_of_destination());
  CHECK(!D3d12CopyFormats(r8g8b8a8_unorm, r8_unorm).plane_of_destination());
  CHECK(!D3d12CopyFormats(d32_float_s8x24, bc1_unorm).plane_of_destination());
  CHECK(!D3d12CopyFormats(d32_float_s8x24, unknown_format).plane_of_destination());
  CHECK(!D3d12CopyFormats(unknown_format, r8_unorm).plane_of_destination()); }
