#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/gl_extension_list.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::GlExtensionList;
using av::glicd::GlExtensionPolicy;
using av::glicd::GuestGlContext;
using Row = GlExtensionPolicy::Row;

namespace av::glicd {

struct GlExtensionPolicyProbe {
    static void blank_wgl(GlExtensionPolicy &policy) { policy.wgl_[0] = '\0'; }
};

}

namespace {

constexpr Row carried_rows[] = {
    {"GL_ARB_texture_float", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_KHR_debug", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_debug_output", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
};
constexpr Row one_row[] = {
    {"GL_ARB_debug_output", 1u, "gl_extension_policy.stops_asking_once_every_row_is_found"},
};
constexpr Row wgl_rows[] = {
    {"WGL_ARB_extensions_string", 1u, "gl_extension_policy.wgl_names_come_from_their_rows_without_tear_control"},
    {"WGL_EXT_swap_control", 1u, "gl_extension_policy.wgl_names_come_from_their_rows_without_tear_control"},
    {"WGL_ARB_pixel_format", 1u, "gl_extension_policy.wgl_names_come_from_their_rows_without_tear_control"},
    {"WGL_EXT_swap_control_tear", 1u, "gl_extension_policy.wgl_names_come_from_their_rows_without_tear_control"},
};

constexpr const char *renderer = "host gpu";

struct Made {
    GlContextRig      rig;
    GlExtensionPolicy empty{nullptr, 0u, nullptr, 0u};
    GuestGlContext    context{rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(), empty, rig.handles(), 1,
                              GlContextOptions()};

    Made()
    {
        rig.host().gl_made(renderer, "4.6.0 host");
        CHECK_STATUS(context.make(), Status::ok);
        CHECK_EQ(context.extensions().count(), 0u);
    }
    av::ClientTransport &wire() { return *context.wire(); }
};

void answer_every_name_with(GlContextRig &rig, const char *name)
{
    std::uint8_t    bytes[av::MockKmt::max_answer];
    av::WireWriter  writer(bytes, sizeof bytes);
    const std::size_t length = std::strlen(name);
    writer.write_varint(1u);
    writer.write_varint(length);
    writer.write(name, length);
    rig.kmt().answer_waits(bytes, static_cast<std::uint32_t>(writer.offset()));
}

}

TEST(gl_extension_policy, reports_what_rows_the_tier_and_the_host_share)
{ Made made;
  const GlExtensionPolicy policy(carried_rows, 3u, nullptr, 0u);
  CHECK(policy.invariants_hold());
  made.rig.host().gl_integer(5);
  made.rig.host().gl_string("GL_ARB_debug_output");
  made.rig.host().gl_string("GL_ARB_debug_output");
  made.rig.host().gl_string("GL_KHR_debug");
  made.rig.host().gl_string("GL_EXT_host_only");
  made.rig.host().gl_string("GL_ARB_texture_float");
  const std::uint32_t before = made.rig.kmt().commands();
  GlExtensionList list(made.rig.platform());
  policy.learn(made.wire(), list);
  CHECK_EQ(list.count(), 2u);
  CHECK(std::strcmp(list.name(0u), "GL_ARB_texture_float") == 0);
  CHECK(std::strcmp(list.name(1u), "GL_ARB_debug_output") == 0);
  CHECK(list.name(0u) == carried_rows[0].name_);
  CHECK(std::strcmp(list.joined(), "GL_ARB_texture_float GL_ARB_debug_output") == 0);
  CHECK_EQ(made.rig.kmt().commands() - before, 6u);
  CHECK_EQ(made.rig.kmt().queued_answers(), 0u);
  CHECK(list.invariants_hold()); }

TEST(gl_extension_policy, stops_asking_once_every_row_is_found)
{ Made made;
  const GlExtensionPolicy policy(one_row, 1u, nullptr, 0u);
  made.rig.host().gl_integer(900);
  made.rig.host().gl_string("GL_ARB_debug_output");
  const std::uint32_t before = made.rig.kmt().commands();
  GlExtensionList list(made.rig.platform());
  policy.learn(made.wire(), list);
  CHECK_EQ(list.count(), 1u);
  CHECK_EQ(made.rig.kmt().commands() - before, 2u); }

TEST(gl_extension_policy, asks_no_more_than_1024_names)
{ Made made;
  const GlExtensionPolicy policy(one_row, 1u, nullptr, 0u);
  made.rig.host().gl_integer(1025);
  answer_every_name_with(made.rig, "GL_EXT_not_it");
  const std::uint32_t before = made.rig.kmt().calls(av::MockKmt::Call::submit);
  GlExtensionList list(made.rig.platform());
  policy.learn(made.wire(), list);
  CHECK_EQ(list.count(), 0u);
  CHECK_EQ(made.rig.kmt().calls(av::MockKmt::Call::submit) - before, 1u + GlExtensionPolicy::max_host_names); }

TEST(gl_extension_policy, a_name_too_long_to_hold_is_no_row_s_name)
{ Made made;
  const GlExtensionPolicy policy(one_row, 1u, nullptr, 0u);
  char long_name[GlExtensionPolicy::name_bytes + 1u];
  std::memset(long_name, 'x', sizeof long_name - 1u);
  std::memcpy(long_name, "GL_ARB_debug_output", 19u);
  long_name[GlExtensionPolicy::name_bytes] = '\0';
  made.rig.host().gl_integer(2);
  made.rig.host().gl_string(long_name);
  made.rig.host().gl_string("GL_EXT_other");
  GlExtensionList list(made.rig.platform());
  policy.learn(made.wire(), list);
  CHECK_EQ(list.count(), 0u);
  Made shorter;
  long_name[GlExtensionPolicy::name_bytes - 1u] = '\0';
  shorter.rig.host().gl_integer(1);
  shorter.rig.host().gl_string(long_name);
  policy.learn(shorter.wire(), list);
  CHECK_EQ(list.count(), 0u); }

TEST(gl_extension_policy, a_failed_answer_stops_the_reading_and_keeps_what_was_found)
{ Made made;
  const GlExtensionPolicy policy(carried_rows, 3u, nullptr, 0u);
  made.rig.host().gl_integer(3);
  made.rig.host().gl_string("GL_ARB_debug_output");
  made.rig.host().cut_next(1u);
  made.rig.host().gl_string("GL_ARB_texture_float");
  GlExtensionList list(made.rig.platform());
  policy.learn(made.wire(), list);
  CHECK_EQ(list.count(), 1u);
  CHECK(std::strcmp(list.joined(), "GL_ARB_debug_output") == 0); }

TEST(gl_extension_policy, no_rows_or_no_host_names_ask_nothing_more)
{ Made made;
  const GlExtensionPolicy none(nullptr, 5u, nullptr, 0u);
  const std::uint32_t before = made.rig.kmt().commands();
  GlExtensionList list(made.rig.platform());
  none.learn(made.wire(), list);
  CHECK_EQ(made.rig.kmt().commands(), before);
  const GlExtensionPolicy policy(one_row, 1u, nullptr, 0u);
  const std::int32_t counts[] = {0, -3};
  for (const std::int32_t listed : counts) {
    made.rig.host().gl_integer(listed);
    const std::uint32_t asked = made.rig.kmt().commands();
    policy.learn(made.wire(), list);
    CHECK_EQ(made.rig.kmt().commands() - asked, 1u);
  }
  CHECK_EQ(list.count(), 0u); }

TEST(gl_extension_policy, wgl_names_come_from_their_rows_without_tear_control)
{ const GlExtensionPolicy policy(nullptr, 0u, wgl_rows, 4u);
  CHECK(std::strcmp(policy.wgl_extensions(),
                    "WGL_ARB_extensions_string WGL_EXT_swap_control WGL_ARB_pixel_format") == 0);
  CHECK(policy.invariants_hold());
  const GlExtensionPolicy nothing(nullptr, 0u, nullptr, 3u);
  CHECK(std::strcmp(nothing.wgl_extensions(), "") == 0);
  const GlExtensionPolicy only_withheld(nullptr, 0u, wgl_rows + 3, 1u);
  CHECK(std::strcmp(only_withheld.wgl_extensions(), "") == 0);
  CHECK(only_withheld.invariants_hold()); }

TEST(gl_extension_policy, reads_no_more_than_64_rows_of_either_table)
{ static Row gl_rows[GlExtensionPolicy::max_rows + 1u];
  static Row long_wgl[GlExtensionPolicy::max_rows + 1u];
  static char names[GlExtensionPolicy::max_rows + 1u][24];
  static char gl_names[GlExtensionPolicy::max_rows + 1u][24];
  for (std::uint32_t at = 0u; at <= GlExtensionPolicy::max_rows; at++) {
    std::snprintf(names[at], sizeof names[at], "WGL_ARB_row_%02u", at);
    long_wgl[at] = Row{names[at], 1u, "suite.case"};
    std::snprintf(gl_names[at], sizeof gl_names[at], "GL_ARB_row_%02u", at);
    gl_rows[at] = Row{gl_names[at], 1u, "suite.case"};
  }
  const GlExtensionPolicy policy(gl_rows, GlExtensionPolicy::max_rows + 1u, long_wgl,
                                 GlExtensionPolicy::max_rows + 1u);
  CHECK(policy.invariants_hold());
  CHECK(std::strstr(policy.wgl_extensions(), "WGL_ARB_row_63") != nullptr);
  CHECK(std::strstr(policy.wgl_extensions(), "WGL_ARB_row_64") == nullptr); }

TEST(gl_extension_policy, wgl_rows_longer_than_the_string_are_cut)
{ static Row rows[GlExtensionPolicy::max_rows];
  static char names[GlExtensionPolicy::max_rows][96];
  for (std::uint32_t at = 0u; at < GlExtensionPolicy::max_rows; at++) {
    std::snprintf(names[at], sizeof names[at], "WGL_ARB_a_rather_long_extension_name_to_fill_the_string_%02u_xxxxxxxxxxxxxxxxxxxxxx", at);
    rows[at] = Row{names[at], 1u, "suite.case"};
  }
  const GlExtensionPolicy policy(nullptr, 0u, rows, GlExtensionPolicy::max_rows);
  CHECK_EQ(std::strlen(policy.wgl_extensions()), GlExtensionPolicy::wgl_bytes - 1u);
  CHECK(policy.invariants_hold()); }

TEST(gl_extension_policy_invariants, fail_for_rows_of_the_wrong_kind_or_a_string_that_is_not_theirs)
{ constexpr Row wgl_as_gl[] = {{"WGL_ARB_pixel_format", 1u, "suite.case"}};
  constexpr Row gl_as_wgl[] = {{"GL_ARB_debug_output", 1u, "suite.case"}};
  constexpr Row unnamed[] = {{nullptr, 1u, "suite.case"}};
  CHECK(!GlExtensionPolicy(wgl_as_gl, 1u, nullptr, 0u).invariants_hold());
  CHECK(!GlExtensionPolicy(unnamed, 1u, nullptr, 0u).invariants_hold());
  CHECK(!GlExtensionPolicy(nullptr, 0u, gl_as_wgl, 1u).invariants_hold());
  CHECK(!GlExtensionPolicy(nullptr, 0u, unnamed, 1u).invariants_hold());
  GlExtensionPolicy blanked(nullptr, 0u, wgl_rows, 1u);
  av::glicd::GlExtensionPolicyProbe::blank_wgl(blanked);
  CHECK(!blanked.invariants_hold());
  CHECK(GlExtensionPolicy(nullptr, 0u, nullptr, 0u).invariants_hold()); }
