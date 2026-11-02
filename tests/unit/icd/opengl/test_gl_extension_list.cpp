#include "harness/framework/test_registry.hpp"
#include "icd/opengl/gl_extension_list.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::glicd::GlExtensionList;

namespace av::glicd {

struct GlExtensionListProbe {
    static void overcount(GlExtensionList &list) { list.count_ = GlExtensionList::max_names + 1u; }
    static void unterminate(GlExtensionList &list) { list.joined_[list.length_] = 'x'; }
    static void lengthen(GlExtensionList &list) { list.length_ = GlExtensionList::joined_bytes; }
    static void miscount(GlExtensionList &list) { list.joined_[list.length_ = 1u] = '\0'; }
    static void empty_name(GlExtensionList &list) { list.names_[0] = ""; }
};

}

TEST(gl_extension_list, joins_the_names_it_is_given_once_each)
{ GlExtensionList list;
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
  CHECK(list.invariants_hold());
  CHECK_EQ(list.count(), 2u);
  CHECK(list.name(0u) == first);
  CHECK(list.name(1u) == second);
  CHECK(list.name(2u) == nullptr);
  CHECK(std::strcmp(list.joined(), "GL_KHR_debug GL_ARB_debug_output") == 0); }

TEST(gl_extension_list, stops_at_its_name_count_and_its_string_room)
{ GlExtensionList full;
  static char names[GlExtensionList::max_names + 1u][8];
  for (std::uint32_t at = 0u; at <= GlExtensionList::max_names; at++) {
    std::snprintf(names[at], sizeof names[at], "GL_%u", at);
    CHECK(full.add(names[at]) == (at < GlExtensionList::max_names));
  }
  CHECK_EQ(full.count(), GlExtensionList::max_names);
  CHECK(full.invariants_hold());
  GlExtensionList room;
  static char long_names[3][2049];
  std::memset(long_names[0], 'a', 2047u);
  std::memset(long_names[1], 'b', 2048u);
  CHECK(room.add(long_names[0]));
  CHECK(!room.add(long_names[1]));
  long_names[1][2047] = '\0';
  CHECK(room.add(long_names[1]));
  CHECK_EQ(std::strlen(room.joined()), GlExtensionList::joined_bytes - 1u);
  long_names[2][0] = 'c';
  CHECK(!room.add(long_names[2]));
  CHECK(room.invariants_hold()); }

TEST(gl_extension_list_invariants, fail_when_the_string_and_the_names_disagree)
{ GlExtensionList counted;
  av::glicd::GlExtensionListProbe::overcount(counted);
  CHECK(!counted.invariants_hold());
  GlExtensionList unterminated;
  av::glicd::GlExtensionListProbe::unterminate(unterminated);
  CHECK(!unterminated.invariants_hold());
  GlExtensionList long_string;
  av::glicd::GlExtensionListProbe::lengthen(long_string);
  CHECK(!long_string.invariants_hold());
  GlExtensionList miscounted;
  CHECK(miscounted.add("GL_KHR_debug"));
  av::glicd::GlExtensionListProbe::miscount(miscounted);
  CHECK(!miscounted.invariants_hold());
  GlExtensionList emptied;
  CHECK(emptied.add("GL_KHR_debug"));
  av::glicd::GlExtensionListProbe::empty_name(emptied);
  CHECK(!emptied.invariants_hold()); }
