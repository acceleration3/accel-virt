#include "executor/shim/scrgb_rows.hpp"
#include "harness/framework/test_registry.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

using av::ScrgbRows;

namespace {
constexpr std::uint32_t r32g32b32a32_float = 2u;
constexpr std::uint32_t r32g32b32_float = 6u;
constexpr std::uint32_t r16g16b16a16_float = 10u;
constexpr std::uint32_t r16g16_float = 34u;
constexpr std::uint32_t r8g8b8a8_unorm = 28u;
constexpr std::uint32_t b8g8r8a8_unorm = 87u;
constexpr std::uint32_t r10g10b10a2_unorm = 24u;
constexpr std::uint32_t r8_unorm = 61u;

std::uint16_t half(float value)
{ std::uint32_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(bits));
  const std::uint32_t sign = (bits >> 16u) & 0x8000u;
  const std::int32_t  exponent = static_cast<std::int32_t>((bits >> 23u) & 0xFFu) - 127 + 15;
  const std::uint32_t fraction = (bits >> 13u) & 0x3FFu;
  if (value == 0.0f) return static_cast<std::uint16_t>(sign);
  if (exponent >= 31) return static_cast<std::uint16_t>(sign | 0x7C00u);
  if (exponent <= 0) return static_cast<std::uint16_t>(sign | static_cast<std::uint32_t>(std::lround(std::fabs(value) * 16777216.0f)));
  return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10u) | fraction); }

int reference(double linear)
{ if (!(linear > 0.0)) return 0;
  if (linear > 1.0) linear = 1.0;
  const double encoded = linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
  return static_cast<int>(std::floor(encoded * 255.0 + 0.5)); }

void convert_half(const ScrgbRows &rows, const float (&colour)[4], std::uint8_t (&out)[4])
{ std::uint16_t pixel[4];
  for (int i = 0; i < 4; i++) pixel[i] = half(colour[i]);
  rows.convert(reinterpret_cast<const std::uint8_t *>(pixel), 1u, out); }
}

TEST(scrgb_rows, the_checks_half_float_colour_encodes_to_the_srgb_bytes_windows_shows)
{ ScrgbRows rows;
  CHECK(rows.use(r16g16b16a16_float, b8g8r8a8_unorm));
  CHECK_EQ(rows.pixel_bytes(), 8u);
  const float colour[4] = {0.2f, 0.4f, 0.6f, 1.0f};
  std::uint8_t out[4] = {};
  convert_half(rows, colour, out);
  CHECK_EQ(static_cast<int>(out[2]), 124);
  CHECK_EQ(static_cast<int>(out[1]), 170);
  CHECK_EQ(static_cast<int>(out[0]), 203);
  CHECK_EQ(static_cast<int>(out[3]), 255); }

TEST(scrgb_rows, a_red_first_target_swaps_the_red_and_blue_bytes)
{ ScrgbRows rows;
  CHECK(rows.use(r16g16b16a16_float, r8g8b8a8_unorm));
  const float colour[4] = {0.2f, 0.4f, 0.6f, 1.0f};
  std::uint8_t out[4] = {};
  convert_half(rows, colour, out);
  CHECK_EQ(static_cast<int>(out[0]), 124);
  CHECK_EQ(static_cast<int>(out[1]), 170);
  CHECK_EQ(static_cast<int>(out[2]), 203); }

TEST(scrgb_rows, values_outside_zero_to_one_clamp_and_not_a_numbers_are_black)
{ ScrgbRows rows;
  CHECK_EQ(static_cast<int>(rows.encode(-3.0f)), 0);
  CHECK_EQ(static_cast<int>(rows.encode(0.0f)), 0);
  CHECK_EQ(static_cast<int>(rows.encode(1.0f)), 255);
  CHECK_EQ(static_cast<int>(rows.encode(7.5f)), 255);
  CHECK_EQ(static_cast<int>(rows.encode(std::nanf(""))), 0);
  CHECK_EQ(static_cast<int>(rows.encode(INFINITY)), 255); }

TEST(scrgb_rows, the_table_stays_within_one_step_of_the_exact_curve_over_the_whole_range)
{ ScrgbRows rows;
  int worst = 0;
  for (int step = 0; step <= 10000; step++) {
    const double linear = step / 10000.0;
    const int    difference = std::abs(static_cast<int>(rows.encode(static_cast<float>(linear))) - reference(linear));
    if (difference > worst) worst = difference; }
  CHECK(worst <= 1); }

TEST(scrgb_rows, half_precision_values_decode_including_denormals_negatives_and_infinity)
{ ScrgbRows rows;
  CHECK(rows.use(r16g16b16a16_float, b8g8r8a8_unorm));
  const float tiny[4] = {0.001f, -0.5f, 65504.0f, 0.5f};
  std::uint8_t out[4] = {};
  convert_half(rows, tiny, out);
  CHECK_EQ(static_cast<int>(out[2]), reference(0.0009999275207519531));
  CHECK_EQ(static_cast<int>(out[1]), 0);
  CHECK_EQ(static_cast<int>(out[0]), 255);
  CHECK_EQ(static_cast<int>(out[3]), 128);
  const std::uint16_t denormal[4] = {0x0001u, 0x03FFu, 0x7C00u, 0xFC00u};
  rows.convert(reinterpret_cast<const std::uint8_t *>(denormal), 1u, out);
  CHECK_EQ(static_cast<int>(out[2]), reference(5.960464477539063e-08));
  CHECK_EQ(static_cast<int>(out[1]), reference(6.097555160522461e-05));
  CHECK_EQ(static_cast<int>(out[0]), 255);
  CHECK_EQ(static_cast<int>(out[3]), 0); }

TEST(scrgb_rows, single_precision_rows_convert_pixel_by_pixel_with_missing_channels_filled)
{ ScrgbRows rows;
  CHECK(rows.use(r32g32b32a32_float, b8g8r8a8_unorm));
  CHECK_EQ(rows.pixel_bytes(), 16u);
  const float two[8] = {0.2f, 0.4f, 0.6f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f};
  std::uint8_t out[8] = {};
  rows.convert(reinterpret_cast<const std::uint8_t *>(two), 2u, out);
  CHECK_EQ(static_cast<int>(out[2]), 124);
  CHECK_EQ(static_cast<int>(out[3]), 128);
  CHECK_EQ(static_cast<int>(out[6]), 255);
  CHECK_EQ(static_cast<int>(out[4]), 0);
  CHECK(rows.use(r32g32b32_float, b8g8r8a8_unorm));
  CHECK_EQ(rows.pixel_bytes(), 12u);
  const float three[3] = {0.2f, 0.4f, 0.6f};
  rows.convert(reinterpret_cast<const std::uint8_t *>(three), 1u, out);
  CHECK_EQ(static_cast<int>(out[1]), 170);
  CHECK_EQ(static_cast<int>(out[3]), 255);
  CHECK(rows.use(r16g16_float, b8g8r8a8_unorm));
  CHECK_EQ(rows.pixel_bytes(), 4u);
  const std::uint16_t pair[2] = {half(0.2f), half(0.6f)};
  rows.convert(reinterpret_cast<const std::uint8_t *>(pair), 1u, out);
  CHECK_EQ(static_cast<int>(out[2]), 124);
  CHECK_EQ(static_cast<int>(out[1]), 203);
  CHECK_EQ(static_cast<int>(out[0]), 0);
  CHECK_EQ(static_cast<int>(out[3]), 255); }

TEST(scrgb_rows, sources_and_targets_the_rows_cannot_make_are_refused)
{ ScrgbRows rows;
  CHECK(!rows.use(r8g8b8a8_unorm, b8g8r8a8_unorm));
  CHECK(!rows.use(r16g16b16a16_float, r10g10b10a2_unorm));
  CHECK(!rows.use(r16g16b16a16_float, r8_unorm));
  CHECK_EQ(rows.pixel_bytes(), 0u);
  CHECK(rows.use(r16g16b16a16_float, b8g8r8a8_unorm)); }
