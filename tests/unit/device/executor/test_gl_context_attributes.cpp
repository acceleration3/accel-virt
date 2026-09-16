/* The EGL attribute list an OpenGL context is made with, from the options the
 * guest asked for (feature 050): each option as the EGL 1.5 attribute that
 * means it, nothing for what was not asked, and options no OpenGL has refused
 * before EGL is asked. Judged without a GPU. */
#include "device/abi/rvk_executor.h"
#include "device/executor/gl_context_attributes.hpp"
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

/* The value `name` has in the list, or -1 when it is not there. */
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

}  // namespace

TEST(gl_context_attributes, no_options_are_an_empty_list)
{ const GlContextAttributes defaults(nullptr);
  CHECK(defaults.valid());
  CHECK_EQ(defaults.list()[0], none);
  const RvkContextOptions zero = {};
  const GlContextAttributes zeroed(&zero);
  CHECK(zeroed.valid());
  CHECK_EQ(pairs_in(zeroed), 0u); }

TEST(gl_context_attributes, every_option_becomes_its_egl_attribute)
{ const RvkContextOptions everything = {4u, 6u, RVK_CONTEXT_PROFILE_CORE,
                                        RVK_CONTEXT_FLAG_DEBUG | RVK_CONTEXT_FLAG_FORWARD_COMPATIBLE |
                                            RVK_CONTEXT_FLAG_ROBUST_ACCESS | RVK_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET,
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
{ const RvkContextOptions compat = {2u, 1u, RVK_CONTEXT_PROFILE_COMPATIBILITY, 0u, 0u};
  const GlContextAttributes compatibility(&compat);
  CHECK_EQ(pairs_in(compatibility), 3u);
  CHECK_EQ(value_of(compatibility, profile_mask), 2);
  CHECK_EQ(value_of(compatibility, debug), -1);
  const RvkContextOptions plain = {3u, 3u, 0u, RVK_CONTEXT_FLAG_ROBUST_ACCESS, 0u};
  const GlContextAttributes no_profile(&plain);
  CHECK_EQ(pairs_in(no_profile), 3u);
  CHECK_EQ(value_of(no_profile, profile_mask), -1);
  CHECK_EQ(value_of(no_profile, robust_access), 1);
  CHECK_EQ(value_of(no_profile, reset_strategy), -1); }

TEST(gl_context_attributes, options_no_opengl_has_are_invalid_and_list_nothing)
{ const RvkContextOptions refused[] = {
      {5u, 0u, 1u, 0u, 0u}, {4u, 7u, 1u, 0u, 0u}, {3u, 4u, 1u, 0u, 0u}, {2u, 2u, 0u, 0u, 0u},
      {1u, 6u, 0u, 0u, 0u}, {4u, 6u, 3u, 0u, 0u}, {4u, 6u, 1u, 0x10u, 0u}, {4u, 6u, 1u, 0u, 1u},
      {0u, 1u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u}, {0u, 0u, 0u, 1u, 0u}, {0u, 0u, 0u, 0u, 1u},
  };
  for (const RvkContextOptions &options : refused) {
      const GlContextAttributes attributes(&options);
      CHECK(!attributes.valid());
      CHECK_EQ(attributes.list()[0], none);
  }
  const RvkContextOptions highest[] = {{1u, 5u, 0u, 0u, 0u}, {2u, 1u, 0u, 0u, 0u}, {3u, 3u, 2u, 15u, 0u},
                                       {4u, 0u, 1u, 0u, 0u}};
  for (const RvkContextOptions &options : highest) CHECK(GlContextAttributes(&options).valid()); }
