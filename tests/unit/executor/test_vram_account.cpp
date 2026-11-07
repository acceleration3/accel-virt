#include "executor/vram_account.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::VramAccount;
using av::VramLedger;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(vram_account, an_account_with_no_ledger_takes_everything_and_reports_no_cap)
{ VramAccount account;
  CHECK(!account.bound());
  CHECK(account.take(1000 * gib));
  CHECK_EQ(account.cap(), UINT64_MAX);
  CHECK_EQ(account.free_bytes(), UINT64_MAX); }

TEST(vram_account, two_accounts_on_one_ledger_share_one_cap)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  VramAccount first(ledger);
  VramAccount second(ledger);
  CHECK(first.take(8 * gib));
  CHECK(second.take(6 * gib));
  CHECK(!first.take(1u));
  CHECK(!second.take(1u));
  CHECK_EQ(first.held(), 8 * gib);
  CHECK_EQ(second.held(), 6 * gib);
  CHECK_EQ(ledger.used(), 14 * gib); }

TEST(vram_account, an_account_cannot_give_back_more_than_it_took)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  VramAccount first(ledger);
  VramAccount second(ledger);
  CHECK(first.take(gib));
  CHECK(second.take(2 * gib));
  first.give(3 * gib);
  CHECK_EQ(first.held(), 0u);
  CHECK_EQ(ledger.used(), 2 * gib); }

TEST(vram_account, closing_returns_what_a_dead_device_leaked_and_says_how_much)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  VramAccount account(ledger);
  CHECK(account.take(3 * gib));
  account.give(gib);
  CHECK_EQ(account.close(), 2 * gib);
  CHECK_EQ(ledger.used(), 0u);
  CHECK_EQ(account.close(), 0u); }

TEST(vram_account, rebinding_closes_the_old_ledgers_share_first)
{ VramLedger old_ledger;
  VramLedger new_ledger;
  old_ledger.learn(16 * gib);
  new_ledger.learn(16 * gib);
  VramAccount account(old_ledger);
  CHECK(account.take(gib));
  account.bind(&new_ledger);
  CHECK_EQ(old_ledger.used(), 0u);
  CHECK(account.take(2 * gib));
  CHECK_EQ(new_ledger.used(), 2 * gib); }

TEST(vram_account, a_refused_take_leaves_the_account_and_ledger_as_they_were)
{ VramLedger ledger;
  ledger.learn(2 * gib);
  VramAccount account(ledger);
  CHECK(account.take(gib));
  CHECK(!account.take(gib));
  CHECK_EQ(account.held(), gib);
  CHECK_EQ(ledger.used(), gib); }
