#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_extension_list.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::MockPlatform;
using av::glicd::GlExtensionList;
using av::Status;

namespace av::glicd {

struct GlExtensionListProbe {
    static void name_only(GlExtensionList &list, const char *name) { static_cast<void>(list.names_.add(name)); }
    static void word_only(GlExtensionList &list, const char *name) { static_cast<void>(list.joined_.add(name)); }
};

}

TEST(gl_extension_list, joins_the_names_it_is_given_once_each)
{ MockPlatform platform;
  GlExtensionList list(platform);
  CHECK_EQ(list.count(), 0u);
  CHECK(std::strcmp(list.joined(), "") == 0);
  CHECK(list.name(0u) == nullptr);
  CHECK(list.invariants_hold());
  static const char first[] = "GL_KHR_debug";
  static const char second[] = "GL_ARB_debug_output";
  CHECK(list.add(first));
  CHECK(list.invariants_hold());
  CHECK(list.add(second));
  CHECK(!list.add("GL_KHR_debug"));
  CHECK(!list.add(""));
  CHECK(!list.add(nullptr));
  CHECK(!list.add("GL_with space"));
  CHECK(list.invariants_hold());
  CHECK_EQ(list.count(), 2u);
  CHECK(list.name(0u) == first);
  CHECK(list.name(1u) == second);
  CHECK(list.name(2u) == nullptr);
  CHECK(std::strcmp(list.joined(), "GL_KHR_debug GL_ARB_debug_output") == 0); }

TEST(gl_extension_list, grows_past_any_fixed_count_and_string_room)
{ MockPlatform platform;
  GlExtensionList list(platform);
  constexpr std::uint32_t many = 1100u;
  static char names[many][16];
  std::size_t joined_bytes = 0u;
  for (std::uint32_t at = 0u; at < many; at++) {
    std::snprintf(names[at], sizeof names[at], "GL_EXT_%u", at);
    CHECK(list.add(names[at]));
    joined_bytes += std::strlen(names[at]) + (at != 0u ? 1u : 0u);
  }
  CHECK_EQ(list.count(), many);
  CHECK_EQ(std::strlen(list.joined()), joined_bytes);
  CHECK(list.name(many - 1u) == names[many - 1u]);
  CHECK(list.name(many) == nullptr);
  CHECK(!list.add(names[7]));
  CHECK_EQ(list.count(), many);
  CHECK(list.invariants_hold()); }

TEST(gl_extension_list, a_name_that_cannot_be_stored_is_refused_and_changes_nothing)
{ for (std::uint32_t refused = 1u; refused <= 2u; refused++) {
    MockPlatform platform;
    GlExtensionList list(platform);
    CHECK(list.add("GL_KHR_debug"));
    const std::uint32_t before = list.count();
    platform.fail_allocation_at(refused);
    bool failed = false;
    static char names[1100][16];
    for (std::uint32_t at = 0u; at < 1100u && !failed; at++) {
      std::snprintf(names[at], sizeof names[at], "GL_EXT_%u", at);
      failed = !list.add(names[at]);
      CHECK(list.invariants_hold());
      if (failed) CHECK_EQ(list.count(), before + at);
    }
    CHECK(failed);
    platform.fail_allocation_at(0u);
    CHECK(list.add("GL_AFTER_refusal"));
    CHECK(list.invariants_hold());
  } }

TEST(gl_extension_list_invariants, fail_when_the_string_and_the_names_disagree)
{ MockPlatform platform;
  GlExtensionList named(platform);
  CHECK(named.add("GL_KHR_debug"));
  av::glicd::GlExtensionListProbe::name_only(named, "GL_ARB_debug_output");
  CHECK(!named.invariants_hold());
  GlExtensionList worded(platform);
  CHECK(worded.add("GL_KHR_debug"));
  av::glicd::GlExtensionListProbe::word_only(worded, "GL_ARB_debug_output");
  CHECK(!worded.invariants_hold());
  GlExtensionList reordered(platform);
  av::glicd::GlExtensionListProbe::name_only(reordered, "GL_B");
  av::glicd::GlExtensionListProbe::word_only(reordered, "GL_A");
  av::glicd::GlExtensionListProbe::name_only(reordered, "GL_A");
  av::glicd::GlExtensionListProbe::word_only(reordered, "GL_B");
  CHECK(!reordered.invariants_hold()); }
