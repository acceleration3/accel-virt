#include "core/platform/log_budget.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::Irql;
using av::LogBudget;
using av::MockPlatform;
using av::Violation;

TEST(log_budget_allows, admits_a_windows_worth_of_lines_then_refuses)
{ MockPlatform platform;
  LogBudget budget(platform);
  CHECK(budget.invariants_hold());
  for (std::uint32_t line = 0u; line < LogBudget::lines_per_window; line++) {
      CHECK(budget.allows());
      CHECK(budget.invariants_hold());
  }
  CHECK(!budget.allows());
  CHECK_EQ(budget.dropped(), 1u);
  CHECK(!budget.allows());
  CHECK_EQ(budget.dropped(), 2u);
  CHECK(budget.invariants_hold()); }

TEST(log_budget_allows, admits_again_once_the_window_has_passed)
{ MockPlatform platform;
  LogBudget budget(platform);
  for (std::uint32_t line = 0u; line < LogBudget::lines_per_window; line++) CHECK(budget.allows());
  CHECK(!budget.allows());
  platform.advance_time(LogBudget::window_ticks - 1u);
  CHECK(!budget.allows());
  CHECK_EQ(budget.dropped(), 2u);
  platform.advance_time(1u);
  CHECK(budget.allows());
  CHECK_EQ(budget.dropped(), 2u);
  CHECK(budget.invariants_hold()); }

TEST(log_budget_dropped, counts_every_refusal_across_windows)
{ MockPlatform platform;
  LogBudget budget(platform);
  for (std::uint32_t burst = 0u; burst < 3u; burst++) {
      for (std::uint32_t line = 0u; line < LogBudget::lines_per_window + 10u; line++)
          static_cast<void>(budget.allows());
      platform.advance_time(LogBudget::window_ticks);
  }
  CHECK_EQ(budget.dropped(), 30u);
  CHECK(budget.invariants_hold()); }

TEST(log_budget_allows, neither_allocates_nor_blocks_at_dispatch)
{ MockPlatform platform;
  LogBudget budget(platform);
  const std::uint32_t before = platform.allocations_made();
  platform.set_irql(Irql::dispatch);
  for (std::uint32_t line = 0u; line < 200u; line++) static_cast<void>(budget.allows());
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.allocations_made(), before);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  CHECK(budget.invariants_hold()); }

TEST(log_budget_allows, is_dispatch_at_most)
{ MockPlatform platform;
  LogBudget budget(platform);
  platform.set_irql(Irql::isr);
  static_cast<void>(budget.allows());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }
