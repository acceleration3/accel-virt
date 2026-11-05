#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/buffer_clear_pattern.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::GuardedBuffer;
using av::glicd::BufferClearPattern;

namespace {

constexpr std::uint32_t no_error = BufferClearPattern::gl_no_error;
constexpr std::uint32_t invalid_enum = BufferClearPattern::gl_invalid_enum;
constexpr std::uint32_t invalid_operation = BufferClearPattern::gl_invalid_operation;

constexpr std::uint32_t gl_red = 0x1903u, gl_rg = 0x8227u, gl_rgb = 0x1907u, gl_rgba = 0x1908u;
constexpr std::uint32_t gl_bgr = 0x80E0u, gl_bgra = 0x80E1u;
constexpr std::uint32_t gl_red_integer = 0x8D94u, gl_rg_integer = 0x8228u, gl_rgb_integer = 0x8D98u,
                        gl_rgba_integer = 0x8D99u;
constexpr std::uint32_t t_ubyte = 0x1401u, t_byte = 0x1400u, t_ushort = 0x1403u, t_short = 0x1402u, t_uint = 0x1405u,
                        t_int = 0x1404u, t_float = 0x1406u, t_half = 0x140Bu;
constexpr std::uint32_t r8 = 0x8229u, r16 = 0x822Au, r16f = 0x822Du, r32f = 0x822Eu, r8i = 0x8231u, r16i = 0x8233u,
                        r32i = 0x8235u, r8ui = 0x8232u, r16ui = 0x8234u, r32ui = 0x8236u, rg8 = 0x822Bu,
                        rgba8 = 0x8058u, rgba16 = 0x805Bu, rgba16f = 0x881Au, rgba32f = 0x8814u, rgba8i = 0x8D8Eu,
                        rgba8ui = 0x8D7Cu, rgba32ui = 0x8D70u, rgb32f = 0x8815u, rgb32i = 0x8D83u, rgb32ui = 0x8D71u;

struct Converted {
    BufferClearPattern pattern;
    std::uint32_t      error;
    std::uint8_t       bytes[BufferClearPattern::most_bytes];

    Converted(std::uint32_t internal_format, std::uint32_t format, std::uint32_t type, const void *data)
        : pattern(), error(pattern.convert(internal_format, format, type, data)), bytes{}
    {
        std::uint8_t filled[BufferClearPattern::most_bytes * 2u];
        std::memset(filled, 0xEE, sizeof filled);
        if (pattern.size() != 0u) pattern.fill(filled, pattern.size());
        std::memcpy(bytes, filled, BufferClearPattern::most_bytes);
    }
};

std::uint32_t read32(const std::uint8_t *at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, at, sizeof value);
    return value;
}

std::uint16_t read16(const std::uint8_t *at)
{
    std::uint16_t value = 0u;
    std::memcpy(&value, at, sizeof value);
    return value;
}

}

TEST(buffer_clear_pattern, a_fresh_pattern_is_empty_and_fills_nothing)
{ BufferClearPattern pattern;
  CHECK_EQ(pattern.size(), 0u);
  CHECK(pattern.invariants_hold());
  std::uint8_t bytes[8];
  std::memset(bytes, 0x42, sizeof bytes);
  pattern.fill(bytes, sizeof bytes);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0x42u); }

TEST(buffer_clear_pattern_convert, no_data_means_zero_in_the_internal_formats_size)
{ const std::uint32_t internals[] = {r8, r16, r16f, r32f, rg8, rgba8, rgba16f, rgba32f, rgb32f, rgba8ui, r32i};
  const std::uint32_t sizes[] = {1u, 2u, 2u, 4u, 2u, 4u, 8u, 16u, 12u, 4u, 4u};
  for (std::uint32_t index = 0u; index < sizeof internals / sizeof internals[0]; index++) {
      const bool   integer = internals[index] == rgba8ui || internals[index] == r32i;
      const Converted made(internals[index], integer ? gl_red_integer : gl_red, integer ? t_uint : t_ubyte, nullptr);
      CHECK_EQ(made.error, no_error);
      CHECK_EQ(made.pattern.size(), sizes[index]);
      for (std::uint32_t at = 0u; at < sizes[index]; at++) CHECK_EQ(made.bytes[at], 0u);
      CHECK(made.pattern.invariants_hold());
  } }

TEST(buffer_clear_pattern_convert, every_internal_format_has_its_components_times_its_width_as_its_size)
{ struct Row { std::uint32_t internal; std::uint32_t size; bool integer; };
  const Row rows[] = {
      {0x8229u, 1u, false}, {0x822Au, 2u, false}, {0x822Du, 2u, false}, {0x822Eu, 4u, false},
      {0x8231u, 1u, true},  {0x8233u, 2u, true},  {0x8235u, 4u, true},  {0x8232u, 1u, true},
      {0x8234u, 2u, true},  {0x8236u, 4u, true},  {0x822Bu, 2u, false}, {0x822Cu, 4u, false},
      {0x822Fu, 4u, false}, {0x8230u, 8u, false}, {0x8237u, 2u, true},  {0x8239u, 4u, true},
      {0x823Bu, 8u, true},  {0x8238u, 2u, true},  {0x823Au, 4u, true},  {0x823Cu, 8u, true},
      {0x8815u, 12u, false}, {0x8D83u, 12u, true}, {0x8D71u, 12u, true}, {0x8058u, 4u, false},
      {0x805Bu, 8u, false}, {0x881Au, 8u, false}, {0x8814u, 16u, false}, {0x8D8Eu, 4u, true},
      {0x8D88u, 8u, true},  {0x8D82u, 16u, true}, {0x8D7Cu, 4u, true},  {0x8D76u, 8u, true},
      {0x8D70u, 16u, true},
  };
  for (const Row &row : rows) {
      const std::uint32_t data[4] = {1u, 1u, 1u, 1u};
      const Converted     made(row.internal, row.integer ? gl_rgba_integer : gl_rgba, row.integer ? t_uint : t_ubyte,
                               data);
      CHECK_EQ(made.error, no_error);
      CHECK_EQ(made.pattern.size(), row.size);
      CHECK(made.pattern.invariants_hold());
  } }

TEST(buffer_clear_pattern_convert, normalized_eight_bit_rounds_a_float_to_the_nearest_step)
{ const float   data[4] = {1.0f, 0.5f, 0.0f, 0.25f};
  const Converted made(rgba8, gl_rgba, t_float, data);
  CHECK_EQ(made.error, no_error);
  CHECK_EQ(made.bytes[0], 255u);
  CHECK_EQ(made.bytes[1], 128u);
  CHECK_EQ(made.bytes[2], 0u);
  CHECK_EQ(made.bytes[3], 64u); }

TEST(buffer_clear_pattern_convert, normalized_values_are_clamped_and_a_nan_is_zero)
{ float       data[4] = {2.0f, -1.0f, 0.0f, 1.0f};
  const float nan = __builtin_nanf("");
  data[2] = nan;
  const Converted made(rgba8, gl_rgba, t_float, data);
  CHECK_EQ(made.bytes[0], 255u);
  CHECK_EQ(made.bytes[1], 0u);
  CHECK_EQ(made.bytes[2], 0u);
  CHECK_EQ(made.bytes[3], 255u);
  const float     infinities[4] = {__builtin_inff(), -__builtin_inff(), 1.0e30f, -1.0e30f};
  const Converted wide(rgba8, gl_rgba, t_float, infinities);
  CHECK_EQ(wide.bytes[0], 255u);
  CHECK_EQ(wide.bytes[1], 0u);
  CHECK_EQ(wide.bytes[2], 255u);
  CHECK_EQ(wide.bytes[3], 0u); }

TEST(buffer_clear_pattern_convert, normalized_sixteen_bit_uses_the_full_range)
{ const float   one[1] = {1.0f};
  const Converted full(r16, gl_red, t_float, one);
  CHECK_EQ(read16(full.bytes), 0xFFFFu);
  const std::uint16_t half_one = 0x3C00u;
  const Converted     from_half(r16, gl_red, t_half, &half_one);
  CHECK_EQ(read16(from_half.bytes), 0xFFFFu);
  const float     half_value[1] = {0.5f};
  const Converted middle(r16, gl_red, t_float, half_value);
  CHECK_EQ(read16(middle.bytes), 32768u); }

TEST(buffer_clear_pattern_convert, integer_sources_normalize_by_their_own_range)
{ const std::uint8_t full[1] = {255u};
  CHECK_EQ(Converted(r8, gl_red, t_ubyte, full).bytes[0], 255u);
  CHECK_EQ(Converted(r16, gl_red, t_ubyte, full).bytes[0], 0xFFu);
  CHECK_EQ(read16(Converted(r16, gl_red, t_ubyte, full).bytes), 0xFFFFu);
  const std::uint16_t middle[1] = {32768u};
  CHECK_EQ(Converted(r8, gl_red, t_ushort, middle).bytes[0], 128u);
  const std::uint32_t top[1] = {0xFFFFFFFFu};
  CHECK_EQ(Converted(r8, gl_red, t_uint, top).bytes[0], 255u);
  const std::int8_t positive[1] = {127};
  CHECK_EQ(Converted(r8, gl_red, t_byte, positive).bytes[0], 255u);
  const std::int8_t lowest[1] = {-128};
  CHECK_EQ(Converted(r8, gl_red, t_byte, lowest).bytes[0], 0u);
  const std::int32_t largest[1] = {0x7FFFFFFF};
  CHECK_EQ(Converted(r8, gl_red, t_int, largest).bytes[0], 255u);
  const std::int16_t negative[1] = {-1};
  CHECK_EQ(Converted(r8, gl_red, t_short, negative).bytes[0], 0u); }

TEST(buffer_clear_pattern_convert, floating_targets_keep_a_float_verbatim_and_narrow_a_half_target)
{ const float   data[4] = {1.5f, -2.25f, 65520.0f, 0.1f};
  const Converted wide(rgba32f, gl_rgba, t_float, data);
  float           kept[4] = {};
  std::memcpy(kept, wide.bytes, sizeof kept);
  CHECK(kept[0] == 1.5f);
  CHECK(kept[1] == -2.25f);
  CHECK(kept[2] == 65520.0f);
  CHECK(kept[3] == 0.1f);
  const Converted narrow(rgba16f, gl_rgba, t_float, data);
  CHECK_EQ(read16(narrow.bytes), 0x3E00u);
  CHECK_EQ(read16(narrow.bytes + 2), 0xC080u);
  CHECK_EQ(read16(narrow.bytes + 4), 0x7C00u);
  CHECK_EQ(read16(narrow.bytes + 6), 0x2E66u); }

TEST(buffer_clear_pattern_convert, a_half_source_is_widened_exactly)
{ const std::uint16_t data[1] = {0x3C00u};
  const Converted     wide(r32f, gl_red, t_half, data);
  float               kept = 0.0f;
  std::memcpy(&kept, wide.bytes, sizeof kept);
  CHECK(kept == 1.0f);
  const std::uint16_t same[1] = {0xC080u};
  CHECK_EQ(read16(Converted(r16f, gl_red, t_half, same).bytes), 0xC080u); }

TEST(buffer_clear_pattern_convert, a_float_nan_stays_a_nan_in_a_float_target_and_a_half_target)
{ const float   nan[1] = {__builtin_nanf("")};
  const Converted wide(r32f, gl_red, t_float, nan);
  float           kept = 0.0f;
  std::memcpy(&kept, wide.bytes, sizeof kept);
  CHECK(kept != kept);
  const Converted narrow(r16f, gl_red, t_float, nan);
  CHECK_EQ(read16(narrow.bytes) & 0x7C00u, 0x7C00u);
  CHECK((read16(narrow.bytes) & 0x03FFu) != 0u); }

TEST(buffer_clear_pattern_convert, integer_targets_clamp_to_their_own_range)
{ const std::int32_t big[1] = {100000};
  CHECK_EQ(Converted(r8i, gl_red_integer, t_int, big).bytes[0], 127u);
  CHECK_EQ(read16(Converted(r16i, gl_red_integer, t_int, big).bytes), 32767u);
  const std::int32_t small[1] = {-100000};
  CHECK_EQ(Converted(r8i, gl_red_integer, t_int, small).bytes[0], 0x80u);
  CHECK_EQ(read16(Converted(r16i, gl_red_integer, t_int, small).bytes), 0x8000u);
  const std::uint32_t over[1] = {70000u};
  CHECK_EQ(read16(Converted(r16ui, gl_red_integer, t_uint, over).bytes), 0xFFFFu);
  CHECK_EQ(Converted(r8ui, gl_red_integer, t_uint, over).bytes[0], 0xFFu);
  const std::int32_t negative[1] = {-5};
  CHECK_EQ(Converted(r8ui, gl_red_integer, t_int, negative).bytes[0], 0u);
  CHECK_EQ(read32(Converted(r32i, gl_red_integer, t_int, negative).bytes), 0xFFFFFFFBu);
  const std::uint32_t top[1] = {0xFFFFFFFFu};
  CHECK_EQ(read32(Converted(r32ui, gl_red_integer, t_uint, top).bytes), 0xFFFFFFFFu);
  CHECK_EQ(read32(Converted(r32i, gl_red_integer, t_uint, top).bytes), 0x7FFFFFFFu); }

TEST(buffer_clear_pattern_convert, integer_sources_of_every_width_and_sign_are_read_as_such)
{ const std::uint8_t ub[1] = {200u};
  CHECK_EQ(Converted(r8ui, gl_red_integer, t_ubyte, ub).bytes[0], 200u);
  CHECK_EQ(Converted(r8i, gl_red_integer, t_ubyte, ub).bytes[0], 127u);
  const std::int8_t sb[1] = {-3};
  CHECK_EQ(Converted(r8i, gl_red_integer, t_byte, sb).bytes[0], 0xFDu);
  CHECK_EQ(Converted(r8ui, gl_red_integer, t_byte, sb).bytes[0], 0u);
  const std::uint16_t us[1] = {60000u};
  CHECK_EQ(read16(Converted(r16ui, gl_red_integer, t_ushort, us).bytes), 60000u);
  const std::int16_t ss[1] = {-300};
  CHECK_EQ(read16(Converted(r16i, gl_red_integer, t_short, ss).bytes), static_cast<std::uint16_t>(-300)); }

TEST(buffer_clear_pattern_convert, the_channels_follow_the_layouts_order_and_missing_ones_default)
{ const std::uint8_t bgra[4] = {10u, 20u, 30u, 40u};
  const Converted    swapped(rgba8, gl_bgra, t_ubyte, bgra);
  CHECK_EQ(swapped.bytes[0], 30u);
  CHECK_EQ(swapped.bytes[1], 20u);
  CHECK_EQ(swapped.bytes[2], 10u);
  CHECK_EQ(swapped.bytes[3], 40u);
  const std::uint8_t three[3] = {10u, 20u, 30u};
  const Converted    rgb(rgba8, gl_rgb, t_ubyte, three);
  CHECK_EQ(rgb.bytes[0], 10u);
  CHECK_EQ(rgb.bytes[1], 20u);
  CHECK_EQ(rgb.bytes[2], 30u);
  CHECK_EQ(rgb.bytes[3], 255u);
  const Converted bgr(rgba8, gl_bgr, t_ubyte, three);
  CHECK_EQ(bgr.bytes[0], 30u);
  CHECK_EQ(bgr.bytes[2], 10u);
  CHECK_EQ(bgr.bytes[3], 255u);
  const std::uint8_t two[2] = {10u, 20u};
  const Converted    rg(rgba8, gl_rg, t_ubyte, two);
  CHECK_EQ(rg.bytes[0], 10u);
  CHECK_EQ(rg.bytes[1], 20u);
  CHECK_EQ(rg.bytes[2], 0u);
  CHECK_EQ(rg.bytes[3], 255u);
  const std::uint8_t one[1] = {10u};
  const Converted    red(rgba8, gl_red, t_ubyte, one);
  CHECK_EQ(red.bytes[0], 10u);
  CHECK_EQ(red.bytes[1], 0u);
  CHECK_EQ(red.bytes[3], 255u); }

TEST(buffer_clear_pattern_convert, a_target_with_fewer_components_than_the_data_keeps_the_leading_ones)
{ const std::uint8_t data[4] = {11u, 22u, 33u, 44u};
  const Converted    red(r8, gl_rgba, t_ubyte, data);
  CHECK_EQ(red.pattern.size(), 1u);
  CHECK_EQ(red.bytes[0], 11u);
  const Converted rg(rg8, gl_rgba, t_ubyte, data);
  CHECK_EQ(rg.pattern.size(), 2u);
  CHECK_EQ(rg.bytes[0], 11u);
  CHECK_EQ(rg.bytes[1], 22u);
  const Converted three(rgb32f, gl_rgba, t_ubyte, data);
  CHECK_EQ(three.pattern.size(), 12u); }

TEST(buffer_clear_pattern_convert, integer_targets_default_the_missing_alpha_to_one)
{ const std::uint32_t data[3] = {5u, 6u, 7u};
  const Converted     made(rgba8ui, gl_rgb_integer, t_uint, data);
  CHECK_EQ(made.bytes[0], 5u);
  CHECK_EQ(made.bytes[1], 6u);
  CHECK_EQ(made.bytes[2], 7u);
  CHECK_EQ(made.bytes[3], 1u);
  const std::uint32_t two[2] = {5u, 6u};
  const Converted     rg(rgba32ui, gl_rg_integer, t_uint, two);
  CHECK_EQ(read32(rg.bytes), 5u);
  CHECK_EQ(read32(rg.bytes + 4), 6u);
  CHECK_EQ(read32(rg.bytes + 8), 0u);
  CHECK_EQ(read32(rg.bytes + 12), 1u); }

TEST(buffer_clear_pattern_convert, an_unknown_internal_format_format_or_type_is_an_invalid_enum_and_leaves_no_pattern)
{ const std::uint8_t data[4] = {1u, 2u, 3u, 4u};
  const Converted    bad_internal(0x1234u, gl_rgba, t_ubyte, data);
  CHECK_EQ(bad_internal.error, invalid_enum);
  CHECK_EQ(bad_internal.pattern.size(), 0u);
  CHECK(bad_internal.pattern.invariants_hold());
  const Converted bad_format(rgba8, 0x1234u, t_ubyte, data);
  CHECK_EQ(bad_format.error, invalid_enum);
  CHECK_EQ(bad_format.pattern.size(), 0u);
  const Converted bad_type(rgba8, gl_rgba, 0x1234u, data);
  CHECK_EQ(bad_type.error, invalid_enum);
  CHECK_EQ(bad_type.pattern.size(), 0u);
  const Converted nothing(0u, 0u, 0u, nullptr);
  CHECK_EQ(nothing.error, invalid_enum);
  const Converted max(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, data);
  CHECK_EQ(max.error, invalid_enum);
  CHECK(max.pattern.invariants_hold()); }

TEST(buffer_clear_pattern_convert, a_format_that_does_not_match_the_internal_kind_is_an_invalid_operation)
{ const std::uint8_t data[4] = {1u, 2u, 3u, 4u};
  CHECK_EQ(Converted(rgba8ui, gl_rgba, t_ubyte, data).error, invalid_operation);
  CHECK_EQ(Converted(rgba8, gl_rgba_integer, t_ubyte, data).error, invalid_operation);
  CHECK_EQ(Converted(r32f, gl_red_integer, t_uint, data).error, invalid_operation);
  CHECK_EQ(Converted(r8i, gl_red, t_byte, data).error, invalid_operation);
  CHECK_EQ(Converted(r8ui, gl_rg_integer, t_ubyte, data).error, no_error); }

TEST(buffer_clear_pattern_convert, an_integer_internal_format_refuses_float_and_half_data)
{ const float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  CHECK_EQ(Converted(rgba8ui, gl_rgba_integer, t_float, data).error, invalid_operation);
  CHECK_EQ(Converted(rgba8i, gl_rgba_integer, t_half, data).error, invalid_operation);
  CHECK_EQ(Converted(rgb32i, gl_rgb_integer, t_float, data).error, invalid_operation);
  CHECK_EQ(Converted(rgb32ui, gl_rgb_integer, t_half, data).error, invalid_operation);
  const Converted refused(rgba8ui, gl_rgba_integer, t_float, data);
  CHECK_EQ(refused.pattern.size(), 0u);
  CHECK(refused.pattern.invariants_hold()); }

TEST(buffer_clear_pattern_convert, a_failed_conversion_forgets_the_pattern_the_last_good_one_made)
{ BufferClearPattern pattern;
  const std::uint8_t data[4] = {9u, 9u, 9u, 9u};
  CHECK_EQ(pattern.convert(rgba8, gl_rgba, t_ubyte, data), no_error);
  CHECK_EQ(pattern.size(), 4u);
  CHECK_EQ(pattern.convert(0x1234u, gl_rgba, t_ubyte, data), invalid_enum);
  CHECK_EQ(pattern.size(), 0u);
  CHECK(pattern.invariants_hold());
  std::uint8_t out[8];
  std::memset(out, 0x11, sizeof out);
  pattern.fill(out, sizeof out);
  for (const std::uint8_t byte : out) CHECK_EQ(byte, 0x11u); }

TEST(buffer_clear_pattern_convert, converting_again_replaces_every_byte_of_the_old_pattern)
{ BufferClearPattern pattern;
  const std::uint32_t wide[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
  CHECK_EQ(pattern.convert(rgba32ui, gl_rgba_integer, t_uint, wide), no_error);
  const std::uint8_t one[1] = {1u};
  CHECK_EQ(pattern.convert(r8ui, gl_red_integer, t_ubyte, one), no_error);
  CHECK_EQ(pattern.size(), 1u);
  std::uint8_t out[16];
  std::memset(out, 0x77, sizeof out);
  pattern.fill(out, sizeof out);
  for (const std::uint8_t byte : out) CHECK_EQ(byte, 1u); }

TEST(buffer_clear_pattern_fill, repeats_the_pattern_and_leaves_a_partial_tail_alone)
{ BufferClearPattern pattern;
  const std::uint8_t data[4] = {1u, 2u, 3u, 4u};
  CHECK_EQ(pattern.convert(rgba8, gl_rgba, t_ubyte, data), no_error);
  std::uint8_t out[11];
  std::memset(out, 0xAA, sizeof out);
  pattern.fill(out, 10u);
  const std::uint8_t expected[11] = {1u, 2u, 3u, 4u, 1u, 2u, 3u, 4u, 0xAAu, 0xAAu, 0xAAu};
  CHECK(std::memcmp(out, expected, sizeof out) == 0); }

TEST(buffer_clear_pattern_fill, a_range_shorter_than_one_pattern_or_empty_writes_nothing)
{ BufferClearPattern pattern;
  const float data[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  CHECK_EQ(pattern.convert(rgba32f, gl_rgba, t_float, data), no_error);
  std::uint8_t out[16];
  std::memset(out, 0x33, sizeof out);
  pattern.fill(out, 0u);
  pattern.fill(out, 15u);
  for (const std::uint8_t byte : out) CHECK_EQ(byte, 0x33u);
  pattern.fill(out, 16u);
  CHECK(read32(out) != 0x33333333u); }

TEST(buffer_clear_pattern_fill, never_writes_past_the_bytes_it_was_given)
{ BufferClearPattern pattern;
  const std::uint16_t data[4] = {1u, 2u, 3u, 4u};
  CHECK_EQ(pattern.convert(rgba16, gl_rgba, t_ushort, data), no_error);
  for (std::uint64_t bytes = 0u; bytes <= 40u; bytes++) {
      GuardedBuffer buffer(static_cast<std::size_t>(bytes) != 0u ? static_cast<std::size_t>(bytes) : 1u);
      pattern.fill(buffer.data(), bytes);
      CHECK(buffer.guards_intact());
  } }

TEST(buffer_clear_pattern_sweep, every_internal_format_with_every_layout_and_type_holds_its_invariants_and_stays_in_bounds)
{ const std::uint32_t internals[] = {
      0x8229u, 0x822Au, 0x822Du, 0x822Eu, 0x8231u, 0x8233u, 0x8235u, 0x8232u, 0x8234u, 0x8236u, 0x822Bu,
      0x822Cu, 0x822Fu, 0x8230u, 0x8237u, 0x8239u, 0x823Bu, 0x8238u, 0x823Au, 0x823Cu, 0x8815u, 0x8D83u,
      0x8D71u, 0x8058u, 0x805Bu, 0x881Au, 0x8814u, 0x8D8Eu, 0x8D88u, 0x8D82u, 0x8D7Cu, 0x8D76u, 0x8D70u};
  const std::uint32_t layouts[] = {gl_red, gl_rg, gl_rgb, gl_rgba, gl_bgr, gl_bgra,
                                   gl_red_integer, gl_rg_integer, gl_rgb_integer, gl_rgba_integer};
  const std::uint32_t types[] = {t_ubyte, t_byte, t_ushort, t_short, t_uint, t_int, t_float, t_half};
  std::uint32_t accepted = 0u;
  for (const std::uint32_t internal : internals) {
      for (const std::uint32_t layout : layouts) {
          for (const std::uint32_t type : types) {
              std::uint8_t data[16];
              std::memset(data, 0xFF, sizeof data);
              Converted made(internal, layout, type, data);
              CHECK(made.pattern.invariants_hold());
              CHECK(made.pattern.size() <= BufferClearPattern::most_bytes);
              if (made.error == no_error) accepted++;
              else CHECK_EQ(made.pattern.size(), 0u);
              std::memset(data, 0x00, sizeof data);
              Converted zero(internal, layout, type, data);
              CHECK_EQ(zero.error, made.error);
          }
      }
  }
  CHECK(accepted > 0u); }

TEST(buffer_clear_pattern_sweep, a_name_displaced_from_any_known_name_is_refused)
{ const std::uint8_t data[4] = {1u, 2u, 3u, 4u};
  const std::uint32_t known_internals[] = {r8, rgba8, r32f, rgba32ui};
  for (const std::uint32_t internal : known_internals) {
      CHECK_EQ(Converted(internal + 0x10000u, gl_rgba, t_ubyte, data).error == no_error, false);
      CHECK_EQ(Converted(internal ^ 0x8000u, gl_rgba, t_ubyte, data).error == no_error, false);
  }
  CHECK_EQ(Converted(rgba8, gl_rgba + 1u, t_ubyte, data).error, invalid_enum);
  CHECK_EQ(Converted(rgba8, gl_rgba, t_ubyte + 100u, data).error, invalid_enum); }
