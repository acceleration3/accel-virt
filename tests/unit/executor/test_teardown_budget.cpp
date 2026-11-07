#include "executor/teardown_budget.hpp"
#include "harness/executor/scripted_clock.hpp"
#include "harness/framework/test_registry.hpp"

using av::ScriptedClock;
using av::TeardownBudget;

TEST(teardown_budget, starts_with_all_of_its_time_and_spends_it_with_the_clock)
{ ScriptedClock clock(1000u, 0u);
  TeardownBudget budget(clock, 500u);
  CHECK_EQ(budget.remaining(), 500u);
  CHECK(!budget.expired());
  clock.advance(200u);
  CHECK_EQ(budget.remaining(), 300u);
  CHECK(!budget.expired()); }

TEST(teardown_budget, is_expired_at_its_deadline_and_stays_at_nothing_after_it)
{ ScriptedClock clock(1000u, 0u);
  TeardownBudget budget(clock, 500u);
  clock.advance(500u);
  CHECK_EQ(budget.remaining(), 0u);
  CHECK(budget.expired());
  clock.advance(10000u);
  CHECK_EQ(budget.remaining(), 0u);
  CHECK(budget.expired()); }

TEST(teardown_budget, a_budget_of_nothing_is_expired_at_once)
{ ScriptedClock clock(1000u, 0u);
  TeardownBudget budget(clock, 0u);
  CHECK(budget.expired()); }
