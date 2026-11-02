#include "executor/monotonic_clock.hpp"
#include "executor/poll_budget.hpp"
#include "harness/executor/scripted_clock.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostClock;
using av::MonotonicClock;
using av::PollBudget;
using av::ScriptedClock;

TEST(poll_budget, the_first_poll_is_at_once)
{ ScriptedClock clock(1000u, 0u);
  PollBudget    budget(clock, 0u, 50000u, 0u);
  CHECK(budget.invariants_hold());
  CHECK(budget.next());
  CHECK_EQ(clock.pauses(), 0u);
  CHECK(!budget.expired());
  CHECK(budget.invariants_hold()); }

TEST(poll_budget, a_zero_deadline_is_one_poll)
{ ScriptedClock clock(1000u, 1u);
  PollBudget    budget(clock, 0u, 50000u, 0u);
  CHECK(budget.next());
  CHECK(!budget.next());
  CHECK(budget.expired());
  CHECK(!budget.next());
  CHECK_EQ(clock.pauses(), 0u);
  CHECK(budget.invariants_hold()); }

TEST(poll_budget, polls_within_the_spin_never_sleep)
{ ScriptedClock clock(0u, 100u);
  PollBudget    budget(clock, 1000u, 50000u, 1000000u);
  CHECK(budget.next());
  for (int poll = 0; poll < 9; poll++) CHECK(budget.next());
  CHECK_EQ(clock.pauses(), 0u);
  CHECK(budget.next());
  CHECK_EQ(clock.pauses(), 1u);
  CHECK(clock.paused() == 50000u);
  CHECK(budget.invariants_hold()); }

TEST(poll_budget, polls_after_the_spin_wait_their_pause_until_the_deadline)
{ ScriptedClock clock(5u, 0u);
  PollBudget    budget(clock, 0u, 200000u, 1000000u);
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK_EQ(clock.pauses(), 5u);
  CHECK(!budget.next());
  CHECK(budget.expired());
  CHECK_EQ(clock.pauses(), 5u);
  CHECK(!budget.next());
  CHECK(budget.invariants_hold()); }

TEST(poll_budget, a_zero_pause_yields_between_polls)
{ ScriptedClock clock(0u, 10u);
  PollBudget    budget(clock, 0u, 0u, 35u);
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(!budget.next());
  CHECK_EQ(clock.pauses(), 3u);
  CHECK_EQ(clock.yields(), 3u); }

TEST(poll_budget, the_deadline_counts_from_the_first_poll)
{ ScriptedClock clock(0xffffffff00000000u, 0u);
  PollBudget    budget(clock, 0u, 400u, 1000u);
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(budget.next());
  CHECK(!budget.next());
  CHECK_EQ(clock.pauses(), 3u); }

TEST(poll_budget, on_the_host_clock_a_wait_is_given_up_once_its_deadline_has_passed)
{ MonotonicClock    monotonic;
  HostClock        &clock = monotonic;
  PollBudget        budget(clock, 0u, 1000000u, 5000000u);
  const std::uint64_t started = clock.now_nanoseconds();
  std::uint32_t     polls = 0u;
  while (budget.next()) polls++;
  CHECK(clock.now_nanoseconds() - started >= 5000000u);
  CHECK(budget.expired());
  CHECK(polls >= 2u && polls <= 6u); }
