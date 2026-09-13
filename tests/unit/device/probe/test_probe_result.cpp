#include "core/support/status.hpp"
#include "core/support/text_buffer.hpp"
#include "device/probe/probe_result.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::ProbeResult;
using av::Status;
using av::TextBuffer;

namespace {

/* What describe() wrote, for a result in whatever state the caller left it. */
const char *described(const ProbeResult &result, char *bytes, std::size_t capacity)
{
    TextBuffer line(bytes, capacity);
    result.describe(line);
    return line.text();
}

}  // namespace

TEST(probe_result, answers_nothing_until_it_is_given_an_answer)
{ ProbeResult result;
  CHECK(!result.ran());
  CHECK(!result.passed());
  CHECK_STATUS(result.reason(), Status::ok);
  CHECK_EQ(result.errno_value(), 0);
  CHECK_EQ(result.measured(), 0u); }

TEST(probe_result_record_pass, carries_what_the_probe_measured)
{ ProbeResult result;
  CHECK_STATUS(result.record_pass(0x100000u), Status::ok);
  CHECK(result.ran());
  CHECK(result.passed());
  CHECK_STATUS(result.reason(), Status::ok);
  CHECK_EQ(result.measured(), 0x100000u); }

TEST(probe_result_record_failure, carries_the_reason_and_the_host_error)
{ ProbeResult result;
  CHECK_STATUS(result.record_failure(Status::not_supported, -2), Status::ok);
  CHECK(result.ran());
  CHECK(!result.passed());
  CHECK_STATUS(result.reason(), Status::not_supported);
  CHECK_EQ(result.errno_value(), -2);
  CHECK_EQ(result.measured(), 0u); }

/* A capability bit whose evidence changed underneath it is worse than no bit,
 * so a result answers once. */
TEST(probe_result_record_pass, a_second_answer_is_refused_and_the_first_stands)
{ ProbeResult passed;
  CHECK_STATUS(passed.record_pass(64u), Status::ok);
  CHECK_STATUS(passed.record_pass(128u), Status::invalid_state);
  CHECK_STATUS(passed.record_failure(Status::malformed, 7), Status::invalid_state);
  CHECK(passed.passed());
  CHECK_EQ(passed.measured(), 64u);
  CHECK_EQ(passed.errno_value(), 0);
  ProbeResult failed;
  CHECK_STATUS(failed.record_failure(Status::no_memory, 12), Status::ok);
  CHECK_STATUS(failed.record_failure(Status::malformed, 5), Status::invalid_state);
  CHECK_STATUS(failed.record_pass(1u), Status::invalid_state);
  CHECK(!failed.passed());
  CHECK_STATUS(failed.reason(), Status::no_memory);
  CHECK_EQ(failed.errno_value(), 12); }

/* A failed probe still has something to say about the path taken instead - the
 * rate of the copy a fallback would do - but a passed one's measurement is its
 * own and nothing overwrites it. */
TEST(probe_result_measure, gives_a_failed_result_a_number_and_leaves_a_passed_one_alone)
{ ProbeResult failed;
  CHECK_STATUS(failed.record_failure(Status::not_supported, 0), Status::ok);
  CHECK_EQ(failed.measured(), 0u);
  failed.measure(4096u);
  CHECK_EQ(failed.measured(), 4096u);
  ProbeResult passed;
  CHECK_STATUS(passed.record_pass(64u), Status::ok);
  passed.measure(4096u);
  CHECK_EQ(passed.measured(), 64u);
  ProbeResult unanswered;
  unanswered.measure(4096u);
  CHECK_EQ(unanswered.measured(), 0u);
  CHECK(!unanswered.ran()); }

TEST(probe_result_describe, names_the_outcome_and_what_it_was_for)
{ char bytes[96];
  ProbeResult unanswered;
  CHECK(std::strcmp(described(unanswered, bytes, sizeof bytes), "not run") == 0);
  ProbeResult passed;
  CHECK_STATUS(passed.record_pass(0x100000u), Status::ok);
  CHECK(std::strcmp(described(passed, bytes, sizeof bytes), "passed, 1048576") == 0);
  ProbeResult failed;
  CHECK_STATUS(failed.record_failure(Status::not_supported, -19), Status::ok);
  CHECK(std::strcmp(described(failed, bytes, sizeof bytes), "failed, status 6, host -19") == 0);
  ProbeResult refused;
  CHECK_STATUS(refused.record_failure(Status::malformed, 0), Status::ok);
  CHECK(std::strcmp(described(refused, bytes, sizeof bytes), "failed, status 9") == 0); }
