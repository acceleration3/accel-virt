/* Which extensions an application is shown, and which it is not.
 *
 * Two questions have to agree before an extension is reported: does this ICD
 * carry it - a row, with the test that proves it - and does the host device
 * have it. The device's answer is one space-separated string that a host may
 * leave unterminated, may pad with spaces, and may fill with names this ICD
 * has never heard of, so the parsing is as much of the point as the policy.
 *
 * The table this ICD ships is empty today, and that is the behaviour under
 * test as much as any row would be: nothing is reported that nothing proves. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_extension_policy.hpp"

#include <cstring>

using av::clicd::ClExtensionPolicy;

namespace {

/* A policy of this shape, whatever the shipped one holds: what the class does
 * with rows has to be stated somewhere, and the shipped table is empty. */
constexpr ClExtensionPolicy::Row rows[] = {
    {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
    {"cl_khr_int64_base_atomics", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
    {"cl_khr_3d_image_writes", 1u, "cl_extension_policy.a_row_the_host_does_not_offer_is_not_reported"},
};
constexpr uint32_t row_count = 3u;

}  // namespace

TEST(cl_extension_policy, an_empty_policy_reports_nothing_whatever_the_host_offers)
{ ClExtensionPolicy policy(nullptr, 0u);
  const char host[] = "cl_khr_fp64 cl_khr_icd cl_khr_gl_sharing";
  char into[128] = {'z'};
  const std::size_t needed = policy.report(host, sizeof host, into, sizeof into);
  /* One byte: the terminator of an empty list. */
  CHECK_EQ(needed, 1u);
  CHECK_EQ(into[0], '\0');
  CHECK_EQ(policy.rows(), 0u);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, a_row_the_host_offers_is_reported)
{ ClExtensionPolicy policy(rows, row_count);
  const char host[] = "cl_khr_fp64 cl_khr_int64_base_atomics cl_khr_icd";
  char into[128] = {};
  const std::size_t needed = policy.report(host, sizeof host, into, sizeof into);
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64 cl_khr_int64_base_atomics"), 0);
  CHECK_EQ(needed, std::strlen(into) + 1u);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, a_row_the_host_does_not_offer_is_not_reported)
{ ClExtensionPolicy policy(rows, row_count);
  /* The host has none of the three; nothing is reported, whatever else it
   * has. */
  const char host[] = "cl_khr_icd cl_khr_byte_addressable_store";
  char into[128] = {'z'};
  CHECK_EQ(policy.report(host, sizeof host, into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0'); }

TEST(cl_extension_policy, an_extension_the_host_offers_and_no_row_names_is_not_reported)
{ ClExtensionPolicy policy(rows, row_count);
  /* This is the rule constitution VI is: the host having it is not enough. */
  const char host[] = "cl_khr_gl_sharing cl_khr_fp16 cl_intel_unified_shared_memory";
  char into[128] = {'z'};
  CHECK_EQ(policy.report(host, sizeof host, into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0');
  CHECK(!policy.reportable("cl_khr_gl_sharing"));
  CHECK(!policy.reportable("cl_khr_fp16")); }

TEST(cl_extension_policy, a_name_is_matched_whole_and_never_as_a_prefix)
{ ClExtensionPolicy policy(rows, row_count);
  /* "cl_khr_fp64_extra" is not "cl_khr_fp64", and neither is "cl_khr_fp6". */
  CHECK(!policy.offered_by("cl_khr_fp64_extra", 18u, "cl_khr_fp64"));
  CHECK(!policy.offered_by("cl_khr_fp6", 11u, "cl_khr_fp64"));
  CHECK(!policy.offered_by("xcl_khr_fp64", 13u, "cl_khr_fp64"));
  CHECK(policy.offered_by("cl_khr_fp64", 12u, "cl_khr_fp64")); }

TEST(cl_extension_policy, a_host_string_padded_with_spaces_is_read_as_a_list)
{ ClExtensionPolicy policy(rows, row_count);
  const char host[] = "   cl_khr_fp64     cl_khr_3d_image_writes   ";
  char into[128] = {};
  policy.report(host, sizeof host, into, sizeof into);
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64 cl_khr_3d_image_writes"), 0); }

TEST(cl_extension_policy, a_host_string_with_no_terminator_is_read_only_as_far_as_it_says)
{ ClExtensionPolicy policy(rows, row_count);
  /* A host that answered a size and then filled a buffer to the brim: the
   * bytes are all there is, and nothing past them is read. */
  const char host[] = {'c', 'l', '_', 'k', 'h', 'r', '_', 'f', 'p', '6', '4'};
  char into[128] = {};
  policy.report(host, sizeof host, into, sizeof into);
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64"), 0);
  /* And a name cut short by the length is not that name. */
  CHECK(!policy.offered_by(host, 10u, "cl_khr_fp64")); }

TEST(cl_extension_policy, an_empty_host_string_offers_nothing)
{ ClExtensionPolicy policy(rows, row_count);
  char into[128] = {'z'};
  CHECK_EQ(policy.report("", 1u, into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0');
  CHECK_EQ(policy.report(nullptr, 0u, into, sizeof into), 1u);
  CHECK(!policy.offered_by(nullptr, 0u, "cl_khr_fp64"));
  CHECK(!policy.offered_by("cl_khr_fp64", 12u, nullptr)); }

TEST(cl_extension_policy, an_answer_that_does_not_fit_writes_nothing_and_says_how_much_it_needs)
{ ClExtensionPolicy policy(rows, row_count);
  const char host[] = "cl_khr_fp64 cl_khr_int64_base_atomics";
  char into[8] = {'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z'};
  const std::size_t needed = policy.report(host, sizeof host, into, sizeof into);
  CHECK(needed > sizeof into);
  CHECK_EQ(into[0], 'z');
  /* Asked with the room it named, it fits exactly. */
  char room[64] = {};
  CHECK_EQ(policy.report(host, sizeof host, room, needed), needed);
  CHECK_EQ(std::strlen(room) + 1u, needed); }

TEST(cl_extension_policy, the_size_may_be_asked_for_with_no_buffer)
{ ClExtensionPolicy policy(rows, row_count);
  const char host[] = "cl_khr_fp64";
  const std::size_t needed = policy.report(host, sizeof host, nullptr, 0u);
  CHECK_EQ(needed, std::strlen("cl_khr_fp64") + 1u); }

TEST(cl_extension_policy, reportable_is_what_the_icd_asks_before_it_acts_on_an_extension)
{ ClExtensionPolicy policy(rows, row_count);
  CHECK(policy.reportable("cl_khr_fp64"));
  CHECK(!policy.reportable("cl_khr_fp64_extra"));
  CHECK(!policy.reportable(""));
  CHECK(!policy.reportable(nullptr)); }

TEST(cl_extension_policy, a_policy_given_no_rows_but_a_count_holds_none)
{ ClExtensionPolicy policy(nullptr, 7u);
  CHECK_EQ(policy.rows(), 0u);
  CHECK(policy.invariants_hold());
  CHECK(!policy.reportable("cl_khr_fp64")); }

TEST(cl_extension_policy, this_icd_reports_no_extension_yet_and_that_is_the_table)
{ /* cl_khr_icd is the loader's own contract rather than something an
   * application enables, and this ICD answers its two queries without
   * claiming it; cl_khr_gl_sharing waits for feature 053. Nothing else is
   * carried. The gate reads the same table. */
  ClExtensionPolicy policy(av::clicd::reported_extensions, 0u);
  CHECK_EQ(policy.rows(), 0u);
  CHECK(policy.invariants_hold()); }
