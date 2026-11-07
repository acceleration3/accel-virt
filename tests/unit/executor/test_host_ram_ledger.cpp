#include "executor/host_ram_ledger.hpp"
#include "executor/vram_account.hpp"
#include "harness/executor/fake_memory_gauge.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <thread>
#include <vector>

using av::FakeMemoryGauge;
using av::HostRamLedger;
using av::VramAccount;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(host_ram_ledger, a_ledger_with_no_budget_answers_to_nobody)
{ HostRamLedger ledger;
  CHECK_EQ(ledger.cap(), UINT64_MAX);
  CHECK(ledger.take(100 * gib));
  CHECK_EQ(ledger.used(), 100 * gib);
  CHECK_EQ(ledger.refusals(), 0u); }

TEST(host_ram_ledger, a_budget_of_zero_means_unlimited)
{ HostRamLedger ledger;
  ledger.budget(4 * gib);
  CHECK(!ledger.take(5 * gib));
  ledger.budget(0u);
  CHECK_EQ(ledger.cap(), UINT64_MAX);
  CHECK_EQ(ledger.budget(), 0u);
  CHECK(ledger.take(5 * gib)); }

TEST(host_ram_ledger, a_take_past_the_budget_is_refused_and_changes_nothing)
{ HostRamLedger ledger;
  ledger.budget(10 * gib);
  CHECK(ledger.take(7 * gib));
  CHECK(!ledger.take(4 * gib));
  CHECK_EQ(ledger.used(), 7 * gib);
  CHECK_EQ(ledger.refusals(), 1u);
  CHECK_EQ(ledger.free_bytes(), 3 * gib);
  CHECK(ledger.take(3 * gib));
  CHECK(!ledger.take(1u)); }

TEST(host_ram_ledger, giving_back_makes_the_room_again_and_cannot_go_below_zero)
{ HostRamLedger ledger;
  ledger.budget(2 * gib);
  CHECK(ledger.take(2 * gib));
  ledger.give(gib);
  CHECK(ledger.take(gib));
  ledger.give(5 * gib);
  CHECK_EQ(ledger.used(), 0u); }

TEST(host_ram_ledger, accounts_of_one_ledger_share_the_budget_and_closing_gives_back_what_they_held)
{ HostRamLedger ledger;
  ledger.budget(10 * gib);
  VramAccount first(ledger);
  VramAccount second(ledger);
  CHECK(first.take(6 * gib));
  CHECK(second.take(4 * gib));
  CHECK(!first.take(1u));
  CHECK_EQ(first.close(), 6 * gib);
  CHECK_EQ(ledger.used(), 4 * gib);
  CHECK(second.take(6 * gib)); }

TEST(host_ram_ledger, racing_takes_never_pass_the_budget)
{ HostRamLedger ledger;
  ledger.budget(1000u);
  std::vector<std::thread> threads;
  std::uint64_t            granted[8] = {};
  for (std::uint32_t id = 0u; id < 8u; id++)
    threads.emplace_back([&ledger, &granted, id] {
      for (std::uint32_t at = 0u; at < 500u; at++)
        if (ledger.take(1u)) granted[id]++;
    });
  for (std::thread &thread : threads) thread.join();
  std::uint64_t total = 0u;
  for (const std::uint64_t one : granted) total += one;
  CHECK_EQ(total, 1000u);
  CHECK_EQ(ledger.used(), 1000u); }

TEST(host_ram_ledger_floor, a_take_that_would_leave_the_host_below_its_floor_is_refused_and_allowed_again_once_memory_recovers)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 10 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  CHECK(ledger.take(5 * gib));
  CHECK(!ledger.take(2 * gib));
  CHECK_EQ(ledger.used(), 5 * gib);
  CHECK_EQ(ledger.refusals(), 1u);
  CHECK_EQ(ledger.tight_refusals(), 1u);
  gauge.set(3 * gib, HostRamLedger::refresh_ms);
  CHECK(!ledger.take(1u));
  gauge.set(9 * gib, HostRamLedger::refresh_ms);
  CHECK(ledger.take(2 * gib));
  CHECK_EQ(ledger.used(), 7 * gib); }

TEST(host_ram_ledger_floor, the_host_is_read_at_most_once_per_refresh_interval_however_many_takes_come)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 100 * gib;
  ledger.watch(&gauge);
  ledger.floor(gib);
  for (std::uint32_t at = 0u; at < 1000u; at++) CHECK(ledger.take(1u));
  CHECK_EQ(gauge.reads, 1u);
  gauge.now += HostRamLedger::refresh_ms - 1u;
  CHECK(ledger.take(1u));
  CHECK_EQ(gauge.reads, 1u);
  gauge.now += 1u;
  CHECK(ledger.take(1u));
  CHECK_EQ(gauge.reads, 2u); }

TEST(host_ram_ledger_floor, takes_between_two_reads_count_against_what_the_last_read_found)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 6 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  CHECK(ledger.take(gib));
  CHECK(ledger.take(gib));
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.reads, 1u); }

TEST(host_ram_ledger_floor, a_take_the_budget_refuses_returns_its_headroom)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 6 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  ledger.budget(gib);
  CHECK(!ledger.take(2 * gib));
  CHECK_EQ(ledger.tight_refusals(), 0u);
  CHECK(ledger.take(gib));
  CHECK_EQ(ledger.available(), 5 * gib); }

TEST(host_ram_ledger_floor, no_floor_no_gauge_or_an_unreadable_host_never_refuses)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 0u;
  ledger.watch(&gauge);
  CHECK(ledger.take(gib));
  CHECK_EQ(gauge.reads, 0u);
  ledger.floor(4 * gib);
  gauge.readable = false;
  CHECK(ledger.take(gib));
  gauge.readable = true;
  gauge.now += HostRamLedger::refresh_ms;
  CHECK(!ledger.take(gib));
  ledger.watch(nullptr);
  CHECK(ledger.take(gib));
  ledger.floor(0u);
  CHECK(ledger.take(gib)); }

TEST(host_ram_ledger_floor, going_tight_asks_the_host_to_give_memory_back_once_per_episode)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 10 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  CHECK(ledger.take(1u));
  CHECK_EQ(gauge.relieved, 0u);
  gauge.set(2 * gib, HostRamLedger::refresh_ms);
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.relieved, 1u);
  gauge.set(2 * gib, HostRamLedger::relieve_ms);
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.relieved, 1u);
  gauge.set(10 * gib, HostRamLedger::refresh_ms);
  CHECK(ledger.take(1u));
  gauge.set(2 * gib, HostRamLedger::refresh_ms);
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.relieved, 2u);
  gauge.set(10 * gib, HostRamLedger::refresh_ms);
  CHECK(ledger.take(1u));
  gauge.set(2 * gib, HostRamLedger::refresh_ms);
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.relieved, 2u);
  gauge.set(10 * gib, HostRamLedger::refresh_ms);
  CHECK(ledger.take(1u));
  gauge.set(2 * gib, HostRamLedger::relieve_ms);
  CHECK(!ledger.take(1u));
  CHECK_EQ(gauge.relieved, 3u); }

TEST(host_ram_ledger_floor, racing_takes_never_pass_what_the_host_has_above_its_floor)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 1500u;
  ledger.watch(&gauge);
  ledger.floor(500u);
  std::vector<std::thread> threads;
  std::uint64_t            granted[8] = {};
  for (std::uint32_t id = 0u; id < 8u; id++)
    threads.emplace_back([&ledger, &granted, id] {
      for (std::uint32_t at = 0u; at < 500u; at++)
        if (ledger.take(1u)) granted[id]++;
    });
  for (std::thread &thread : threads) thread.join();
  std::uint64_t total = 0u;
  for (const std::uint64_t one : granted) total += one;
  CHECK_EQ(total, 1000u); }

TEST(host_ram_ledger_floor, bulk_memory_is_refused_while_the_host_is_below_its_floor_and_session_memory_is_not)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 2 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  ledger.budget(10 * gib);
  CHECK(!ledger.take(1u));
  CHECK_EQ(ledger.tight_refusals(), 1u);
  CHECK(ledger.take_session(gib));
  CHECK_EQ(ledger.used(), gib);
  CHECK_EQ(ledger.tight_refusals(), 1u);
  ledger.give(gib);
  CHECK_EQ(ledger.used(), 0u); }

TEST(host_ram_ledger_floor, session_memory_still_counts_toward_the_budget_and_is_refused_past_it)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 2 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  ledger.budget(2 * gib);
  CHECK(ledger.take_session(2 * gib));
  CHECK(!ledger.take_session(1u));
  CHECK_EQ(ledger.refusals(), 1u);
  CHECK_EQ(ledger.tight_refusals(), 0u);
  CHECK_EQ(gauge.reads, 0u); }

TEST(host_ram_ledger_floor, an_account_takes_session_memory_through_a_tight_host_and_gives_it_back_on_close)
{ HostRamLedger ledger;
  FakeMemoryGauge gauge;
  gauge.free_bytes = 2 * gib;
  ledger.watch(&gauge);
  ledger.floor(4 * gib);
  VramAccount account(ledger);
  CHECK(!account.take(1u));
  CHECK_EQ(account.held(), 0u);
  CHECK(account.take_session(gib));
  CHECK_EQ(account.held(), gib);
  CHECK_EQ(account.close(), gib);
  CHECK_EQ(ledger.used(), 0u); }
