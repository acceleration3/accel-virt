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
using av::clicd::ClExtensionPolicyProbe;

namespace av::clicd {

/* The states a policy is never built in. A row with no name or no test is one
 * nothing could report or judge; a name of no length, or one that runs to the
 * end of its room without a terminator, is read off the end of; and the same
 * name twice is a list no application can parse into a set. */
struct ClExtensionPolicyProbe {
    static void lend(ClExtensionPolicy &policy, const ClExtensionPolicy::Row *rows, uint32_t count)
    { policy.rows_ = rows; policy.count_ = count; }
    static void count_past_the_table(ClExtensionPolicy &policy)
    { policy.count_ = ClExtensionPolicy::max_rows + 1u; }
    static void rows_that_are_not_there(ClExtensionPolicy &policy)
    { policy.rows_ = nullptr; policy.count_ = 1u; }
};

}  // namespace av::clicd

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

TEST(cl_extension_policy, a_row_answered_here_is_reported_whatever_the_host_offers)
{ const ClExtensionPolicy::Row own[] = {
      {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
      {"cl_khr_d3d10_sharing", 1u, "cl_extension_policy.a_row_answered_here_is_reported_whatever_the_host_offers",
       ClExtensionPolicy::answered_here},
  };
  ClExtensionPolicy policy(own, 2u);
  char into[128] = {'z'};
  /* No host at all, and a host with neither: the ICD's own row is still
   * reported, and the host's is not. */
  CHECK_EQ(policy.report(nullptr, 0u, into, sizeof into), std::strlen("cl_khr_d3d10_sharing") + 1u);
  CHECK_EQ(std::strcmp(into, "cl_khr_d3d10_sharing"), 0);
  const char host[] = "cl_khr_icd";
  policy.report(host, sizeof host, into, sizeof into);
  CHECK_EQ(std::strcmp(into, "cl_khr_d3d10_sharing"), 0);
  /* And the marker is the whole host name: a host extension called "-x" is a
   * host extension like any other. */
  const ClExtensionPolicy::Row dashed[] = {
      {"cl_khr_d3d10_sharing", 1u, "cl_extension_policy.a_row_answered_here_is_reported_whatever_the_host_offers",
       "-x"}};
  ClExtensionPolicy borrowed(dashed, 1u);
  CHECK_EQ(borrowed.report(host, sizeof host, into, sizeof into), 1u);
  CHECK(policy.invariants_hold()); }

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

namespace {

/* A name that runs to the end of its room with no terminator: what a policy
 * must never hold, and what the walk must stop at rather than run past. */
char unterminated[ClExtensionPolicy::name_bytes + 1u];
/* One byte longer than a name may be, with room for its terminator. */
char over_long[ClExtensionPolicy::name_bytes + 2u];

const ClExtensionPolicy::Row no_name[] = {
    {nullptr, 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
const ClExtensionPolicy::Row no_test[] = {{"cl_khr_fp64", 1u, nullptr}};
const ClExtensionPolicy::Row empty_name[] = {
    {"", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
const ClExtensionPolicy::Row named_twice[] = {
    {"cl_khr_fp64", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"},
    {"cl_khr_fp64", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};

}  // namespace

namespace {

constexpr char many_name_00[] = "cl_av_many_00";
constexpr char many_name_01[] = "cl_av_many_01";
constexpr char many_name_02[] = "cl_av_many_02";
constexpr char many_name_03[] = "cl_av_many_03";
constexpr char many_name_04[] = "cl_av_many_04";
constexpr char many_name_05[] = "cl_av_many_05";
constexpr char many_name_06[] = "cl_av_many_06";
constexpr char many_name_07[] = "cl_av_many_07";
constexpr char many_name_08[] = "cl_av_many_08";
constexpr char many_name_09[] = "cl_av_many_09";
constexpr char many_name_10[] = "cl_av_many_10";
constexpr char many_name_11[] = "cl_av_many_11";
constexpr char many_name_12[] = "cl_av_many_12";
constexpr char many_name_13[] = "cl_av_many_13";
constexpr char many_name_14[] = "cl_av_many_14";
constexpr char many_name_15[] = "cl_av_many_15";
constexpr char many_name_16[] = "cl_av_many_16";
constexpr char many_name_17[] = "cl_av_many_17";
constexpr char many_name_18[] = "cl_av_many_18";
constexpr char many_name_19[] = "cl_av_many_19";
constexpr char many_name_20[] = "cl_av_many_20";
constexpr char many_name_21[] = "cl_av_many_21";
constexpr char many_name_22[] = "cl_av_many_22";
constexpr char many_name_23[] = "cl_av_many_23";
constexpr char many_name_24[] = "cl_av_many_24";
constexpr char many_name_25[] = "cl_av_many_25";
constexpr char many_name_26[] = "cl_av_many_26";
constexpr char many_name_27[] = "cl_av_many_27";
constexpr char many_name_28[] = "cl_av_many_28";
constexpr char many_name_29[] = "cl_av_many_29";
constexpr char many_name_30[] = "cl_av_many_30";
constexpr char many_name_31[] = "cl_av_many_31";
constexpr char many_name_32[] = "cl_av_many_32";
constexpr char many_name_33[] = "cl_av_many_33";
constexpr char many_name_34[] = "cl_av_many_34";
constexpr char many_name_35[] = "cl_av_many_35";
constexpr char many_name_36[] = "cl_av_many_36";
constexpr char many_name_37[] = "cl_av_many_37";
constexpr char many_name_38[] = "cl_av_many_38";
constexpr char many_name_39[] = "cl_av_many_39";

/* More distinct rows than the table holds, all of them well formed: the
 * clamp is about the count, so the array behind it has to be real. */
constexpr ClExtensionPolicy::Row many[] = {
    {many_name_00, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_01, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_02, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_03, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_04, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_05, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_06, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_07, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_08, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_09, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_10, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_11, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_12, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_13, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_14, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_15, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_16, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_17, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_18, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_19, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_20, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_21, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_22, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_23, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_24, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_25, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_26, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_27, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_28, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_29, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_30, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_31, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_32, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_33, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_34, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_35, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_36, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_37, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_38, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"},
    {many_name_39, 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"}};

}  // namespace

/* More rows than the table holds are clamped rather than read past: the
 * count comes from whoever built the policy, and nothing checks it. */
TEST(cl_extension_policy, more_rows_than_it_holds_are_clamped_to_what_it_holds)
{ ClExtensionPolicy policy(many, sizeof many / sizeof many[0]);
  CHECK(sizeof many / sizeof many[0] > ClExtensionPolicy::max_rows);
  CHECK_EQ(policy.rows(), ClExtensionPolicy::max_rows);
  CHECK(policy.invariants_hold()); }

/* A name longer than the room a name may have stops at that room in every
 * walk that reads one - the match, the sizing pass and the writing pass.
 *
 * Such a row is one the invariant rejects, and that is the point: the walks
 * run over a host string before anything has checked the table, so each of
 * them has to stop at name_bytes on its own rather than trust a terminator
 * to arrive first. */
TEST(cl_extension_policy, a_name_longer_than_a_name_may_be_is_read_no_further)
{ /* One byte past the room, and terminated after it - so the host can offer
   * it and the passes that size and write it are actually reached. */
  for (std::size_t index = 0u; index <= ClExtensionPolicy::name_bytes; ++index)
      over_long[index] = 'x';
  over_long[ClExtensionPolicy::name_bytes + 1u] = '\0';
  const ClExtensionPolicy::Row long_row[] = {
      {over_long, 1u, "cl_extension_policy.a_name_longer_than_a_name_may_be_is_read_no_further"}};
  ClExtensionPolicy policy(long_row, 1u);

  const std::size_t needed = policy.report(over_long, sizeof over_long, nullptr, 0u);
  /* Only the room a name may have is counted, not the whole name. */
  CHECK_EQ(needed, static_cast<std::size_t>(ClExtensionPolicy::name_bytes) + 1u);
  char into[ClExtensionPolicy::name_bytes + 8u] = {};
  CHECK_EQ(policy.report(over_long, sizeof over_long, into, sizeof into), needed);
  CHECK_EQ(std::strlen(into), ClExtensionPolicy::name_bytes);
  /* And the match stops at the same place, so it answers no: the row is not
   * the whole word the host named. */
  CHECK(!policy.reportable(over_long));
  /* The invariant rejects the row, which is what says no policy should ship
   * one - the walks above are what keeps it safe until someone tries. */
  CHECK(!policy.invariants_hold()); }

/* The invariant catches the states a policy is never built in. */
TEST(cl_extension_policy, the_invariant_catches_a_row_nothing_could_report)
{ ClExtensionPolicy shipped(rows, sizeof rows / sizeof rows[0]);
  CHECK(shipped.invariants_hold());

  ClExtensionPolicy counted(rows, 1u);
  ClExtensionPolicyProbe::count_past_the_table(counted);
  CHECK(!counted.invariants_hold());

  ClExtensionPolicy rowless(rows, 1u);
  ClExtensionPolicyProbe::rows_that_are_not_there(rowless);
  CHECK(!rowless.invariants_hold());

  const auto refuses = [](const ClExtensionPolicy::Row *table, unsigned int count) {
      ClExtensionPolicy policy(rows, 1u);
      ClExtensionPolicyProbe::lend(policy, table, count);
      return policy.invariants_hold();
  };
  CHECK(!refuses(no_name, 1u));
  CHECK(!refuses(no_test, 1u));
  CHECK(!refuses(empty_name, 1u));
  CHECK(!refuses(named_twice, 2u));

  for (char &byte : unterminated) byte = 'x';
  const ClExtensionPolicy::Row runs_on[] = {
      {unterminated, 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  CHECK(!refuses(runs_on, 1u)); }
