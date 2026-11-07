#include "executor/opengl/gl_texture_shape.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GlTextureShape;

namespace {

constexpr std::uint32_t texture_1d = 0x0DE0u;
constexpr std::uint32_t texture_2d = 0x0DE1u;
constexpr std::uint32_t texture_3d = 0x806Fu;
constexpr std::uint32_t texture_1d_array = 0x8C18u;
constexpr std::uint32_t texture_2d_array = 0x8C1Au;
constexpr std::uint32_t texture_cube_map = 0x8513u;
constexpr std::uint32_t texture_cube_map_array = 0x9009u;
constexpr std::uint32_t texture_2d_multisample = 0x9100u;
constexpr std::uint32_t proxy_texture_2d = 0x8064u;
constexpr std::uint32_t rgba8 = 0x8058u;
constexpr std::uint32_t r8 = 0x8229u;
constexpr std::uint32_t rgba32f = 0x8814u;
constexpr std::uint32_t dxt1 = 0x83F0u;
constexpr std::uint32_t dxt5 = 0x83F3u;
constexpr std::uint32_t astc_6x6 = 0x93B4u;
constexpr std::uint32_t unlisted = 0x1908u;

}

TEST(gl_texture_shape, a_target_is_tracked_when_it_names_storage_and_has_its_binding_to_ask_after)
{ CHECK_EQ(GlTextureShape(texture_2d, rgba8, 1, 1, 4, 4, 1).binding(), 0x8069u);
  CHECK_EQ(GlTextureShape(texture_cube_map_array, rgba8, 1, 1, 4, 4, 6).binding(), 0x900Au);
  CHECK_EQ(GlTextureShape(texture_2d_multisample, rgba8, 1, 4, 4, 4, 1).binding(), 0x9104u);
  CHECK(GlTextureShape(texture_3d, rgba8, 1, 1, 4, 4, 4).tracked());
  CHECK(!GlTextureShape(proxy_texture_2d, rgba8, 1, 1, 4, 4, 1).tracked());
  CHECK(!GlTextureShape(0x8515u, rgba8, 1, 1, 4, 4, 1).tracked());
  CHECK_EQ(GlTextureShape(proxy_texture_2d, rgba8, 1, 1, 4, 4, 1).binding(), 0u); }

TEST(gl_texture_shape, an_extent_level_count_or_sample_count_below_one_or_a_chain_too_long_is_invalid)
{ CHECK(GlTextureShape(texture_2d, rgba8, 1, 1, 4, 4, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 0, 1, 4, 4, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 1, 0, 4, 4, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 1, 1, 0, 4, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 1, 1, 4, -1, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 1, 1, 4, 4, 0).valid());
  CHECK(GlTextureShape(texture_2d, rgba8, 32, 1, 4, 4, 1).valid());
  CHECK(!GlTextureShape(texture_2d, rgba8, 33, 1, 4, 4, 1).valid());
  CHECK(!GlTextureShape(proxy_texture_2d, rgba8, 1, 1, 4, 4, 1).valid());
  CHECK_EQ(GlTextureShape(texture_2d, rgba8, 0, 1, 4, 4, 1).least_bytes(), UINT64_MAX); }

TEST(gl_texture_shape, a_flat_texture_is_its_texels_and_each_level_halves_down_to_one)
{ CHECK_EQ(GlTextureShape(texture_2d, rgba8, 1, 1, 8, 4, 1).least_bytes(), 8u * 4u * 4u);
  CHECK_EQ(GlTextureShape(texture_2d, rgba8, 4, 1, 8, 4, 1).least_bytes(), (32u + 8u + 2u + 1u) * 4u);
  CHECK_EQ(GlTextureShape(texture_1d, r8, 3, 1, 5, 1, 1).least_bytes(), 5u + 2u + 1u);
  CHECK_EQ(GlTextureShape(texture_2d, rgba32f, 1, 1, 3, 3, 1).least_bytes(), 9u * 16u); }

TEST(gl_texture_shape, array_layers_cube_faces_and_samples_multiply_and_never_halve)
{ CHECK_EQ(GlTextureShape(texture_1d_array, rgba8, 2, 1, 4, 3, 1).least_bytes(), (4u * 3u + 2u * 3u) * 4u);
  CHECK_EQ(GlTextureShape(texture_2d_array, rgba8, 2, 1, 4, 4, 5).least_bytes(), (16u * 5u + 4u * 5u) * 4u);
  CHECK_EQ(GlTextureShape(texture_cube_map, rgba8, 1, 1, 4, 4, 1).least_bytes(), 16u * 6u * 4u);
  CHECK_EQ(GlTextureShape(texture_cube_map_array, rgba8, 1, 1, 4, 4, 12).least_bytes(), 16u * 12u * 4u);
  CHECK_EQ(GlTextureShape(texture_3d, rgba8, 3, 1, 4, 4, 4).least_bytes(), (64u + 8u + 1u) * 4u);
  CHECK_EQ(GlTextureShape(texture_2d_multisample, rgba8, 1, 4, 4, 4, 1).least_bytes(), 16u * 4u * 4u); }

TEST(gl_texture_shape, a_compressed_texture_counts_whole_blocks_and_an_unlisted_format_is_the_widest_texel)
{ CHECK_EQ(GlTextureShape(texture_2d, dxt1, 1, 1, 5, 5, 1).least_bytes(), 2u * 2u * 8u);
  CHECK_EQ(GlTextureShape(texture_2d, dxt5, 1, 1, 4, 4, 1).least_bytes(), 16u);
  CHECK_EQ(GlTextureShape(texture_2d, astc_6x6, 1, 1, 7, 6, 1).least_bytes(), 2u * 1u * 16u);
  CHECK_EQ(GlTextureShape(texture_2d, unlisted, 1, 1, 2, 2, 1).least_bytes(), 4u * 16u); }

TEST(gl_texture_shape, a_footprint_that_overflows_is_the_largest_number_so_no_memory_holds_it)
{ CHECK_EQ(GlTextureShape(texture_3d, rgba32f, 1, 1, 0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF)
                 .least_bytes(),
           UINT64_MAX);
  CHECK_EQ(GlTextureShape(texture_2d, rgba32f, 32, 4096, 0x7FFFFFFFFFFFFFFF, 1, 1).least_bytes(), UINT64_MAX); }

TEST(gl_texture_shape, a_sum_of_levels_past_the_largest_number_is_the_largest_number)
{ constexpr std::int64_t widest = 0x7FFFFFFFFFFFFFFF;
  constexpr std::uint32_t r16 = 0x822Au;
  CHECK_EQ(GlTextureShape(texture_1d, r16, 1, 1, widest, 1, 1).least_bytes(), 0xFFFFFFFFFFFFFFFEull);
  CHECK_EQ(GlTextureShape(texture_1d, r16, 2, 1, widest, 1, 1).least_bytes(), UINT64_MAX); }
