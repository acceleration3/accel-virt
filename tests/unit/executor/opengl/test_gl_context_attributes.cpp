#include "device/abi/av_executor.h"
#include "executor/opengl/gl_context_attributes.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::GlContextAttributes;

namespace {

constexpr std::int32_t major_version = 0x3098;
constexpr std::int32_t minor_version = 0x30FB;
constexpr std::int32_t profile_mask = 0x30FD;
constexpr std::int32_t debug = 0x31B0;
constexpr std::int32_t forward_compatible = 0x31B1;
constexpr std::int32_t robust_access = 0x31B2;
constexpr std::int32_t reset_strategy = 0x31BD;
constexpr std::int32_t lose_context_on_reset = 0x31BF;
constexpr std::int32_t none = 0x3038;

std::int32_t value_of(const GlContextAttributes &attributes, std::int32_t name)
{
    const std::int32_t *const list = attributes.list();
    for (std::uint32_t at = 0u; list[at] != none; at += 2u)
        if (list[at] == name) return list[at + 1u];
    return -1;
}

std::uint32_t pairs_in(const GlContextAttributes &attributes)
{
    std::uint32_t count = 0u;
    while (attributes.list()[2u * count] != none) count++;
    return count;
}

}

TEST(gl_context_attributes, no_options_are_an_empty_list)
{ const GlContextAttributes defaults(nullptr);
  CHECK(defaults.valid());
  CHECK_EQ(defaults.list()[0], none);
  const AvContextOptions zero = {};
  const GlContextAttributes zeroed(&zero);
  CHECK(zeroed.valid());
  CHECK_EQ(pairs_in(zeroed), 0u); }

TEST(gl_context_attributes, every_option_becomes_its_egl_attribute)
{ const AvContextOptions everything = {4u, 6u, AV_CONTEXT_PROFILE_CORE,
                                        AV_CONTEXT_FLAG_DEBUG | AV_CONTEXT_FLAG_FORWARD_COMPATIBLE |
                                            AV_CONTEXT_FLAG_ROBUST_ACCESS | AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET,
                                        0u};
  const GlContextAttributes attributes(&everything);
  CHECK(attributes.valid());
  CHECK_EQ(pairs_in(attributes), 7u);
  CHECK_EQ(value_of(attributes, major_version), 4);
  CHECK_EQ(value_of(attributes, minor_version), 6);
  CHECK_EQ(value_of(attributes, profile_mask), 1);
  CHECK_EQ(value_of(attributes, debug), 1);
  CHECK_EQ(value_of(attributes, forward_compatible), 1);
  CHECK_EQ(value_of(attributes, robust_access), 1);
  CHECK_EQ(value_of(attributes, reset_strategy), lose_context_on_reset); }

TEST(gl_context_attributes, only_what_was_asked_is_listed)
{ const AvContextOptions compat = {2u, 1u, AV_CONTEXT_PROFILE_COMPATIBILITY, 0u, 0u};
  const GlContextAttributes compatibility(&compat);
  CHECK_EQ(pairs_in(compatibility), 3u);
  CHECK_EQ(value_of(compatibility, profile_mask), 2);
  CHECK_EQ(value_of(compatibility, debug), -1);
  const AvContextOptions plain = {3u, 3u, 0u, AV_CONTEXT_FLAG_ROBUST_ACCESS, 0u};
  const GlContextAttributes no_profile(&plain);
  CHECK_EQ(pairs_in(no_profile), 3u);
  CHECK_EQ(value_of(no_profile, profile_mask), -1);
  CHECK_EQ(value_of(no_profile, robust_access), 1);
  CHECK_EQ(value_of(no_profile, reset_strategy), -1); }

TEST(gl_context_attributes, options_no_opengl_has_are_invalid_and_list_nothing)
{ const AvContextOptions refused[] = {
      {5u, 0u, 1u, 0u, 0u}, {4u, 7u, 1u, 0u, 0u}, {3u, 4u, 1u, 0u, 0u}, {2u, 2u, 0u, 0u, 0u},
      {1u, 6u, 0u, 0u, 0u}, {4u, 6u, 3u, 0u, 0u}, {4u, 6u, 1u, 0x10u, 0u},
      {0u, 1u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u}, {0u, 0u, 0u, 1u, 0u},
  };
  for (const AvContextOptions &options : refused) {
      const GlContextAttributes attributes(&options);
      CHECK(!attributes.valid());
      CHECK_EQ(attributes.list()[0], none);
  }
  const AvContextOptions highest[] = {{1u, 5u, 0u, 0u, 0u}, {2u, 1u, 0u, 0u, 0u}, {3u, 3u, 2u, 15u, 0u},
                                       {4u, 0u, 1u, 0u, 0u}};
  for (const AvContextOptions &options : highest) CHECK(GlContextAttributes(&options).valid());
  const AvContextOptions in_domain = {4u, 6u, 1u, 0u, 7u};
  CHECK(GlContextAttributes(&in_domain).valid()); }
