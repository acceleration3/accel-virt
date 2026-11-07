#include "executor/host_ram_ledger.hpp"
#include "executor/vram_account.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <thread>
#include <vector>

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
