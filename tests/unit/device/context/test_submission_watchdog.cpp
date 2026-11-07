#include "device/context/submission_watchdog.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::SubmissionWatchdog;

TEST(submission_watchdog, nothing_waiting_never_overruns)
{ SubmissionWatchdog dog(100u);
  CHECK(!dog.overran(1000000u)); }

TEST(submission_watchdog, waiting_without_progress_overruns_at_the_limit_and_not_before)
{ SubmissionWatchdog dog(100u);
  dog.waiting_at(5u, 1000u);
  CHECK(!dog.overran(1099u));
  CHECK(dog.overran(1100u));
  dog.waiting_at(5u, 1090u);
  CHECK(dog.overran(1100u)); }

TEST(submission_watchdog, progress_restarts_the_clock)
{ SubmissionWatchdog dog(100u);
  dog.waiting_at(5u, 1000u);
  dog.waiting_at(6u, 1090u);
  CHECK(!dog.overran(1150u));
  CHECK(dog.overran(1190u)); }

TEST(submission_watchdog, an_empty_queue_clears_the_wait)
{ SubmissionWatchdog dog(100u);
  dog.waiting_at(5u, 1000u);
  dog.nothing_outstanding();
  CHECK(!dog.overran(5000u));
  dog.waiting_at(5u, 6000u);
  CHECK(!dog.overran(6050u)); }

TEST(submission_watchdog, a_zero_limit_disables_it)
{ SubmissionWatchdog dog(0u);
  dog.waiting_at(1u, 10u);
  CHECK(!dog.overran(UINT64_MAX));
  CHECK_EQ(dog.limit(), std::uint64_t{0}); }

TEST(submission_watchdog, a_clock_reading_of_zero_still_starts_the_wait)
{ SubmissionWatchdog dog(100u);
  dog.waiting_at(1u, 0u);
  CHECK(dog.overran(200u)); }

TEST(submission_watchdog, the_deadline_is_the_wait_start_plus_the_limit_and_zero_when_nothing_waits)
{ SubmissionWatchdog dog(100u);
  CHECK_EQ(dog.deadline(), std::uint64_t{0});
  dog.waiting_at(5u, 1000u);
  CHECK_EQ(dog.deadline(), std::uint64_t{1100});
  dog.nothing_outstanding();
  CHECK_EQ(dog.deadline(), std::uint64_t{0});
  SubmissionWatchdog off(0u);
  off.waiting_at(1u, 10u);
  CHECK_EQ(off.deadline(), std::uint64_t{0}); }

TEST(submission_watchdog, the_default_limit_stays_below_the_kernel_ring_timeout)
{ CHECK(SubmissionWatchdog::default_limit_ns < 2000000000ull); }
