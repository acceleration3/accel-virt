#include "harness/framework/test_registry.hpp"
#include "icd/opengl/half_float.hpp"

#include <cstdint>
#include <cstring>

using av::glicd::HalfFloat;

namespace {

float float_of(std::uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

std::uint32_t bits_of(float value)
{
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

std::uint16_t half(float value) { return HalfFloat(value).bits(); }

bool is_nan(float value) { return value != value; }

constexpr std::uint16_t infinity = 0x7C00u;
constexpr std::uint16_t quiet_nan = 0x7E00u;

}

TEST(half_float_encode, exact_values_have_their_known_encodings)
{ CHECK_EQ(half(0.0f), 0x0000u);
  CHECK_EQ(half(-0.0f), 0x8000u);
  CHECK_EQ(half(1.0f), 0x3C00u);
  CHECK_EQ(half(-1.0f), 0xBC00u);
  CHECK_EQ(half(2.0f), 0x4000u);
  CHECK_EQ(half(-2.0f), 0xC000u);
  CHECK_EQ(half(0.5f), 0x3800u);
  CHECK_EQ(half(1.5f), 0x3E00u);
  CHECK_EQ(half(65504.0f), 0x7BFFu);
  CHECK_EQ(half(-65504.0f), 0xFBFFu);
  CHECK_EQ(half(6.103515625e-5f), 0x0400u);
  CHECK_EQ(half(5.960464477539063e-8f), 0x0001u); }

TEST(half_float_encode, rounds_to_the_nearest_value_for_numbers_that_are_not_exact)
{ CHECK_EQ(half(0.1f), 0x2E66u);
  CHECK_EQ(half(1.0f / 3.0f), 0x3555u);
  CHECK_EQ(half(3.14159274f), 0x4248u);
  CHECK_EQ(half(-0.1f), 0xAE66u); }

TEST(half_float_encode, a_tie_between_two_neighbours_goes_to_the_even_one)
{ CHECK_EQ(half(1.0f + 1.0f / 2048.0f), 0x3C00u);
  CHECK_EQ(half(1.0f + 3.0f / 2048.0f), 0x3C02u);
  CHECK_EQ(half(1.0f + 5.0f / 2048.0f), 0x3C02u);
  CHECK_EQ(half(1.0f + 7.0f / 2048.0f), 0x3C04u); }

TEST(half_float_encode, just_off_a_tie_goes_to_the_nearer_neighbour)
{ CHECK_EQ(half(float_of(bits_of(1.0f + 1.0f / 2048.0f) + 1u)), 0x3C01u);
  CHECK_EQ(half(float_of(bits_of(1.0f + 1.0f / 2048.0f) - 1u)), 0x3C00u);
  CHECK_EQ(half(float_of(bits_of(1.0f + 3.0f / 2048.0f) - 1u)), 0x3C01u); }

TEST(half_float_encode, rounding_up_carries_into_the_exponent)
{ CHECK_EQ(half(float_of(0x3FFFF000u)), 0x4000u);
  CHECK_EQ(half(float_of(0x3FFFE000u)), 0x3FFFu);
  CHECK_EQ(half(float_of(0x3FFFEFFFu)), 0x3FFFu);
  CHECK_EQ(half(float_of(0x3FFFF001u)), 0x4000u); }

TEST(half_float_encode, values_past_the_largest_half_round_to_infinity_with_the_tie_going_up)
{ CHECK_EQ(half(65519.0f), 0x7BFFu);
  CHECK_EQ(half(65519.99f), 0x7BFFu);
  CHECK_EQ(half(65520.0f), infinity);
  CHECK_EQ(half(65536.0f), infinity);
  CHECK_EQ(half(1.0e10f), infinity);
  CHECK_EQ(half(-65520.0f), 0x8000u | infinity);
  CHECK_EQ(half(float_of(0x7F7FFFFFu)), infinity);
  CHECK_EQ(half(float_of(0xFF7FFFFFu)), 0x8000u | infinity); }

TEST(half_float_encode, infinities_keep_their_sign)
{ CHECK_EQ(half(float_of(0x7F800000u)), infinity);
  CHECK_EQ(half(float_of(0xFF800000u)), 0x8000u | infinity); }

TEST(half_float_encode, every_nan_becomes_a_quiet_nan_with_its_sign_and_never_an_infinity)
{ const std::uint32_t nans[] = {0x7F800001u, 0x7FC00000u, 0x7FFFFFFFu, 0x7F800400u, 0x7F802000u, 0x7F801FFFu};
  for (const std::uint32_t nan : nans) {
      CHECK_EQ(half(float_of(nan)), quiet_nan);
      CHECK_EQ(half(float_of(nan | 0x80000000u)), 0x8000u | quiet_nan);
  } }

TEST(half_float_encode, half_subnormals_are_exact_multiples_of_two_to_the_minus_twenty_four)
{ for (std::uint32_t step = 1u; step <= 1023u; step++) {
      const float value = static_cast<float>(step) * 5.960464477539063e-8f;
      CHECK_EQ(half(value), step);
      CHECK_EQ(half(-value), 0x8000u | step);
  } }

TEST(half_float_encode, a_value_below_the_smallest_subnormal_rounds_to_zero_or_to_it_by_the_halfway_mark)
{ CHECK_EQ(half(2.9802322387695313e-8f), 0x0000u);
  CHECK_EQ(half(float_of(bits_of(2.9802322387695313e-8f) + 1u)), 0x0001u);
  CHECK_EQ(half(1.0e-10f), 0x0000u);
  CHECK_EQ(half(-1.0e-10f), 0x8000u);
  CHECK_EQ(half(float_of(0x00000001u)), 0x0000u);
  CHECK_EQ(half(float_of(0x80000001u)), 0x8000u);
  CHECK_EQ(half(float_of(0x00800000u)), 0x0000u); }

TEST(half_float_encode, subnormal_ties_go_to_the_even_neighbour)
{ CHECK_EQ(half(1.5f * 5.960464477539063e-8f), 0x0002u);
  CHECK_EQ(half(2.5f * 5.960464477539063e-8f), 0x0002u);
  CHECK_EQ(half(3.5f * 5.960464477539063e-8f), 0x0004u); }

TEST(half_float_encode, the_largest_subnormal_rounds_up_into_the_smallest_normal)
{ CHECK_EQ(half(1023.5f * 5.960464477539063e-8f), 0x0400u);
  CHECK_EQ(half(1023.25f * 5.960464477539063e-8f), 0x03FFu);
  CHECK_EQ(half(float_of(bits_of(1023.5f * 5.960464477539063e-8f) - 1u)), 0x03FFu); }

TEST(half_float_decode, known_encodings_have_their_known_values)
{ CHECK(HalfFloat(static_cast<std::uint16_t>(0x3C00u)).value() == 1.0f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0xBC00u)).value() == -1.0f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x4000u)).value() == 2.0f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x3800u)).value() == 0.5f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x7BFFu)).value() == 65504.0f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x0400u)).value() == 6.103515625e-5f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x0001u)).value() == 5.960464477539063e-8f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x03FFu)).value() == 1023.0f * 5.960464477539063e-8f);
  CHECK(HalfFloat(static_cast<std::uint16_t>(0x3555u)).value() == 0.333251953125f); }

TEST(half_float_decode, zeros_keep_their_sign)
{ CHECK_EQ(bits_of(HalfFloat(static_cast<std::uint16_t>(0x0000u)).value()), 0x00000000u);
  CHECK_EQ(bits_of(HalfFloat(static_cast<std::uint16_t>(0x8000u)).value()), 0x80000000u); }

TEST(half_float_decode, infinities_and_every_nan_pattern_decode_to_the_matching_class)
{ CHECK_EQ(bits_of(HalfFloat(static_cast<std::uint16_t>(0x7C00u)).value()), 0x7F800000u);
  CHECK_EQ(bits_of(HalfFloat(static_cast<std::uint16_t>(0xFC00u)).value()), 0xFF800000u);
  for (std::uint32_t mantissa = 1u; mantissa < 0x400u; mantissa++) {
      CHECK(is_nan(HalfFloat(static_cast<std::uint16_t>(0x7C00u | mantissa)).value()));
      CHECK(is_nan(HalfFloat(static_cast<std::uint16_t>(0xFC00u | mantissa)).value()));
  } }

TEST(half_float, every_half_survives_a_trip_through_a_float_and_back)
{ for (std::uint32_t pattern = 0u; pattern < 0x10000u; pattern++) {
      const std::uint16_t   bits = static_cast<std::uint16_t>(pattern);
      const float           widened = HalfFloat(bits).value();
      const std::uint16_t   again = HalfFloat(widened).bits();
      const bool            nan = (pattern & 0x7C00u) == 0x7C00u && (pattern & 0x03FFu) != 0u;
      if (nan) {
          CHECK_EQ(again, (pattern & 0x8000u) | quiet_nan);
          continue;
      }
      if (again != bits) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "half %04x came back %04x", pattern, again);
  } }

TEST(half_float, widening_is_monotonic_across_the_positive_halves)
{ float last = -1.0f;
  for (std::uint32_t pattern = 0u; pattern < 0x7C00u; pattern++) {
      const float widened = HalfFloat(static_cast<std::uint16_t>(pattern)).value();
      CHECK(widened > last || pattern == 0u);
      last = widened;
  } }

TEST(half_float, narrowing_never_moves_further_than_half_a_step_from_the_original)
{ for (std::uint32_t pattern = 0x0400u; pattern < 0x7BFFu; pattern += 7u) {
      const float low = HalfFloat(static_cast<std::uint16_t>(pattern)).value();
      const float high = HalfFloat(static_cast<std::uint16_t>(pattern + 1u)).value();
      const float middle = low + (high - low) * 0.25f;
      CHECK_EQ(half(middle), pattern);
      const float upper = low + (high - low) * 0.75f;
      CHECK_EQ(half(upper), pattern + 1u);
  } }
