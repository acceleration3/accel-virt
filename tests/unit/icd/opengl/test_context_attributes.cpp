#include "harness/framework/test_registry.hpp"
#include "icd/opengl/context_attributes.hpp"
#include "proto/context/gl_context_options.hpp"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::glicd::ContextAttributes;
using Profile = av::GlContextOptions::Profile;

namespace {

constexpr std::uint32_t ok = 0u;
constexpr std::uint32_t bad_parameter = 87u;
constexpr std::uint32_t bad_version = 0x2095u;
constexpr std::uint32_t bad_profile = 0x2096u;

constexpr std::int32_t major = 0x2091;
constexpr std::int32_t minor = 0x2092;
constexpr std::int32_t layer = 0x2093;
constexpr std::int32_t flags = 0x2094;
constexpr std::int32_t mask = 0x9126;
constexpr std::int32_t reset = 0x8256;

struct Decoded {
    std::uint32_t    error;
    GlContextOptions options;
};

Decoded decode(const std::int32_t *list)
{
    ContextAttributes attributes(list);
    CHECK(attributes.invariants_hold());
    const std::uint32_t error = attributes.decode();
    CHECK(attributes.invariants_hold());
    return Decoded{error, attributes.options()};
}

bool same(const GlContextOptions &options, std::uint8_t want_major, std::uint8_t want_minor, Profile profile,
          std::uint8_t want_flags)
{
    return options.major() == want_major && options.minor() == want_minor && options.profile() == profile &&
           options.flags() == want_flags && options.share_context() == 0u;
}

}

TEST(context_attributes, a_null_or_empty_list_is_the_hosts_default_context)
{ Decoded decoded = decode(nullptr);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults());
  const std::int32_t empty[] = {0, 12345};
  decoded = decode(empty);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults()); }

TEST(context_attributes, a_version_and_profile_become_the_options)
{ const std::int32_t core46[] = {major, 4, minor, 6, mask, 0x1, 0};
  Decoded decoded = decode(core46);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 4u, 6u, Profile::core, 0u));
  const std::int32_t compat33[] = {mask, 0x2, minor, 3, major, 3, 0};
  decoded = decode(compat33);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 3u, 3u, Profile::compatibility, 0u));
  const std::int32_t plain32[] = {major, 3, minor, 2, 0};
  decoded = decode(plain32);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 3u, 2u, Profile::core, 0u));
  const std::int32_t old_with_mask[] = {major, 3, minor, 1, mask, 0x7, 0};
  decoded = decode(old_with_mask);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 3u, 1u, Profile::none, 0u));
  const std::int32_t minor_only[] = {minor, 5, 0};
  decoded = decode(minor_only);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 1u, 5u, Profile::none, 0u));
  const std::int32_t mask_only[] = {mask, 0x2, 0};
  decoded = decode(mask_only);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults());
  const std::int32_t bad_mask_only[] = {mask, 0x3, 0};
  decoded = decode(bad_mask_only);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults()); }

TEST(context_attributes, flags_and_the_reset_strategy_become_option_flags)
{ const std::int32_t all[] = {flags, 0xF, reset, 0x8252, major, 4, minor, 5, 0};
  Decoded decoded = decode(all);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 4u, 5u, Profile::core,
             GlContextOptions::debug | GlContextOptions::forward_compatible | GlContextOptions::robust_access |
                 GlContextOptions::lose_context_on_reset));
  const std::int32_t debug_only[] = {flags, 0x1, 0};
  decoded = decode(debug_only);
  CHECK_EQ(decoded.error, ok);
  CHECK(same(decoded.options, 1u, 0u, Profile::none, GlContextOptions::debug));
  const std::int32_t no_notification[] = {reset, 0x8261, 0};
  decoded = decode(no_notification);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults());
  const std::int32_t main_plane[] = {layer, 0, 0};
  decoded = decode(main_plane);
  CHECK_EQ(decoded.error, ok);
  CHECK(decoded.options.defaults()); }

TEST(context_attributes, every_version_opengl_has_is_accepted_and_no_other)
{ const std::uint8_t last_minor[] = {0u, 5u, 1u, 3u, 6u};
  for (std::int32_t want_major = -1; want_major <= 6; want_major++) {
      for (std::int32_t want_minor = -1; want_minor <= 8; want_minor++) {
          const std::int32_t list[] = {major, want_major, minor, want_minor, 0};
          const Decoded decoded = decode(list);
          const bool exists = want_major >= 1 && want_major <= 4 && want_minor >= 0 &&
                              want_minor <= last_minor[want_major];
          CHECK_EQ(decoded.error, exists ? ok : bad_version);
          if (exists) CHECK(decoded.options.valid());
      }
  } }

TEST(context_attributes, the_hostile_table_answers_the_extensions_errors)
{ struct Case {
      std::int32_t  list[12];
      std::uint32_t error;
  };
  const Case cases[] = {
      {{major, 0, 0}, bad_version},
      {{major, 4, minor, 7, 0}, bad_version},
      {{major, 5, 0}, bad_version},
      {{major, 3, minor, 4, 0}, bad_version},
      {{major, 1, minor, 6, 0}, bad_version},
      {{major, 2, minor, 2, 0}, bad_version},
      {{major, -2147483647 - 1, 0}, bad_version},
      {{major, 2147483647, 0}, bad_version},
      {{major, 2, minor, 1, flags, 0x2, 0}, bad_version},
      {{flags, 0x10, 0}, bad_version},
      {{flags, -1, 0}, bad_version},
      {{major, 4, minor, 6, mask, 3, 0}, bad_profile},
      {{major, 4, minor, 6, mask, 0, 0}, bad_profile},
      {{major, 3, minor, 2, mask, 4, 0}, bad_profile},
      {{flags, 0x2, 0}, bad_version},
      {{layer, 1, 0}, bad_parameter},
      {{layer, -1, 0}, bad_parameter},
      {{reset, 0, 0}, bad_parameter},
      {{reset, 0x8253, 0}, bad_parameter},
      {{0x2090, 0, 0}, bad_parameter},
      {{0x2097, 1, 0}, bad_parameter},
      {{-1, 0, 0}, bad_parameter},
      {{major, 4, major, 4, 0}, bad_parameter},
      {{minor, 1, minor, 1, 0}, bad_parameter},
      {{layer, 0, layer, 0, 0}, bad_parameter},
      {{flags, 0, flags, 1, 0}, bad_parameter},
      {{mask, 1, mask, 1, 0}, bad_parameter},
      {{reset, 0x8261, reset, 0x8261, 0}, bad_parameter},
      {{major, 9, 0x1234, 0, 0}, bad_parameter},
  };
  for (const Case &check : cases) CHECK_EQ(decode(check.list).error, check.error); }

TEST(context_attributes, reads_no_more_than_64_pairs_and_their_terminator)
{
  std::int32_t list[2u * 64u + 1u] = {};
  for (std::uint32_t pair = 0u; pair < 64u; pair++) list[2u * pair] = 0x2091;
  list[128] = 0x2092;
  CHECK_EQ(decode(list).error, bad_parameter);
  std::int32_t unterminated[2u * 64u + 1u] = {};
  for (std::uint32_t pair = 0u; pair < 64u; pair++) unterminated[2u * pair] = 0x7000 + static_cast<std::int32_t>(pair);
  unterminated[128] = 0x7fff;
  CHECK_EQ(decode(unterminated).error, bad_parameter); }

TEST(context_attributes, a_byte_flip_of_a_valid_list_is_answered_and_never_accepts_an_invalid_option)
{ const std::int32_t valid[] = {major, 4, minor, 6, mask, 1, flags, 5, reset, 0x8252, layer, 0, 0};
  std::uint8_t bytes[sizeof valid] = {};
  std::memcpy(bytes, valid, sizeof valid);
  std::uint32_t refused = 0u;
  for (std::uint32_t at = 0u; at < sizeof bytes; at++) {
      for (std::uint32_t bit = 0u; bit < 8u; bit++) {
          std::uint8_t flipped[sizeof valid] = {};
          std::memcpy(flipped, bytes, sizeof bytes);
          flipped[at] = static_cast<std::uint8_t>(flipped[at] ^ (1u << bit));
          std::int32_t list[sizeof valid / sizeof valid[0] + 1u] = {};
          std::memcpy(list, flipped, sizeof flipped);
          const Decoded decoded = decode(list);
          if (decoded.error != ok) refused++;
          else CHECK(decoded.options.valid());
      }
  }
  CHECK(refused > 0u); }

namespace av::glicd {

struct ContextAttributesProbe {
    static std::uint32_t &seen(ContextAttributes &attributes) { return attributes.seen_; }
    static std::int32_t  &value(ContextAttributes &attributes, std::uint32_t index) { return attributes.values_[index]; }
    static bool          &accepted(ContextAttributes &attributes) { return attributes.accepted_; }
};

}

TEST(context_attributes, invariants_refuse_an_acceptance_the_list_does_not_support)
{ using av::glicd::ContextAttributesProbe;
  { ContextAttributes attributes(nullptr);
    CHECK_EQ(attributes.decode(), ok);
    ContextAttributesProbe::seen(attributes) = 1u << 6u;
    CHECK(!attributes.invariants_hold()); }
  { ContextAttributes attributes(nullptr);
    CHECK_EQ(attributes.decode(), ok);
    ContextAttributesProbe::value(attributes, 1u) = 3;
    CHECK(!attributes.invariants_hold()); }
  { const std::int32_t list[] = {major, 4, 0};
    ContextAttributes attributes(list);
    CHECK_EQ(attributes.decode(), ok);
    ContextAttributesProbe::value(attributes, 0u) = 9;
    CHECK(!attributes.invariants_hold()); }
  { const std::int32_t list[] = {major, 9, 0};
    ContextAttributes attributes(list);
    CHECK_EQ(attributes.decode(), bad_version);
    CHECK(attributes.invariants_hold());
    ContextAttributesProbe::accepted(attributes) = true;
    CHECK(!attributes.invariants_hold()); } }
