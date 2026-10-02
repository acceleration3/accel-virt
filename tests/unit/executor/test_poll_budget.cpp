/* A polled wait (PollBudget): exactly as many polls as it was given, the
 * first at once and each later one after its pause, and none after the last -
 * a waiter on a hung GPU gives up rather than hang its caller. */
#include "executor/poll_budget.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <time.h>

using av::PollBudget;

namespace {

std::uint64_t now_nanoseconds()
{
    timespec at = {};
    clock_gettime(CLOCK_MONOTONIC, &at);
    return static_cast<std::uint64_t>(at.tv_sec) * 1000000000u + static_cast<std::uint64_t>(at.tv_nsec);
}

}  // namespace

TEST(poll_budget, gives_exactly_its_polls_and_then_none)
{ PollBudget budget(1000u, 3u);
  CHECK_EQ(budget.polls(), 0u);
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK_EQ(budget.polls(), 3u);
  CHECK(!budget.next());
  CHECK(!budget.next());
  CHECK_EQ(budget.polls(), 3u); }

TEST(poll_budget, no_polls_is_none_at_all)
{ PollBudget budget(1000u, 0u);
  CHECK(!budget.next());
  CHECK_EQ(budget.polls(), 0u); }

/* The first poll is at once; the pauses come between polls. */
TEST(poll_budget, the_first_poll_is_at_once_and_the_rest_wait_their_pause)
{ PollBudget          budget(5000000u, 3u);
  const std::uint64_t started = now_nanoseconds();
  CHECK(budget.next());
  CHECK(now_nanoseconds() - started < 5000000u);
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(now_nanoseconds() - started >= 10000000u); }
