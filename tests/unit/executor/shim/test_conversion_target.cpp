#include "executor/shim/conversion_target.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ConversionTarget;

namespace {
constexpr std::uint32_t r16g16b16a16_float = 10u;
constexpr std::uint32_t r10g10b10a2_unorm = 24u;
constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t r8g8b8a8_srgb = 29u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t b8g8r8a8_srgb = 91u;
constexpr std::uint32_t b8g8r8a8_typeless = 90u;
constexpr std::uint32_t nv12 = 103u;
constexpr std::uint32_t r32_float = 41u;
}

TEST(conversion_target, ten_bit_colour_is_decoded_into_an_unencoded_bgra_target)
{ const ConversionTarget target(r10g10b10a2_unorm, b8g8r8a8_unorm);
  CHECK(target.convertible());
  CHECK(!target.raw_copy());
  CHECK(!target.packed_bytes());
  CHECK_EQ(target.format(), b8g8r8a8_unorm); }

TEST(conversion_target, half_float_colour_is_drawn_into_an_unencoded_bgra_target_for_the_rows_to_encode)
{ const ConversionTarget target(r16g16b16a16_float, b8g8r8a8_unorm);
  CHECK(target.convertible());
  CHECK(!target.raw_copy());
  CHECK_EQ(target.format(), b8g8r8a8_unorm);
  CHECK_EQ(ConversionTarget(r16g16b16a16_float, b8g8r8a8_typeless).format(), b8g8r8a8_unorm); }

TEST(conversion_target, a_value_already_encoded_is_not_encoded_again)
{ CHECK(ConversionTarget(b8g8r8a8_srgb, b8g8r8a8_unorm).raw_copy());
  CHECK(ConversionTarget(r8g8b8a8_srgb, r8g8b8a8_unorm).raw_copy());
  CHECK_EQ(ConversionTarget(r8g8b8a8_srgb, b8g8r8a8_unorm).format(), b8g8r8a8_srgb);
  CHECK_EQ(ConversionTarget(r10g10b10a2_unorm, b8g8r8a8_srgb).format(), b8g8r8a8_srgb); }

TEST(conversion_target, eight_bit_sources_stay_packed_for_the_layout_to_swap)
{ CHECK(ConversionTarget(r8g8b8a8_unorm, b8g8r8a8_unorm).packed_bytes());
  CHECK(ConversionTarget(r8g8b8a8_srgb, b8g8r8a8_unorm).packed_bytes());
  CHECK(ConversionTarget(b8g8r8a8_unorm, b8g8r8a8_unorm).packed_bytes());
  CHECK(!ConversionTarget(r16g16b16a16_float, b8g8r8a8_unorm).packed_bytes()); }

TEST(conversion_target, same_format_blts_are_raw_copies)
{ CHECK(ConversionTarget(b8g8r8a8_unorm, b8g8r8a8_typeless).raw_copy());
  CHECK(ConversionTarget(r10g10b10a2_unorm, r10g10b10a2_unorm).raw_copy());
  CHECK(!ConversionTarget(r8g8b8a8_unorm, b8g8r8a8_unorm).raw_copy()); }

TEST(conversion_target, formats_the_host_cannot_draw_are_not_convertible)
{ CHECK(!ConversionTarget(nv12, b8g8r8a8_unorm).convertible());
  CHECK(!ConversionTarget(b8g8r8a8_unorm, nv12).convertible());
  CHECK(!ConversionTarget(r32_float, b8g8r8a8_unorm).convertible());
  CHECK(ConversionTarget(r8g8b8a8_unorm, b8g8r8a8_unorm).convertible()); }
