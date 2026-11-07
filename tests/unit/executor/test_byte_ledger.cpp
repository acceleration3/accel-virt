#include "executor/byte_ledger.hpp"
#include "executor/host_ram_ledger.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ByteLedger;
using av::HostRamLedger;
using av::VramLedger;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(byte_ledger, a_new_ledger_has_no_cap_and_nothing_used)
{ HostRamLedger ledger;
  const ByteLedger &base = ledger;
  CHECK_EQ(base.cap(), UINT64_MAX);
  CHECK_EQ(base.used(), 0u);
  CHECK_EQ(base.refusals(), 0u);
  CHECK_EQ(base.free_bytes(), UINT64_MAX); }

TEST(byte_ledger, both_kinds_of_ledger_count_through_the_one_lock_free_take_and_give)
{ HostRamLedger host;
  VramLedger    vram;
  host.budget(4 * gib);
  vram.learn(16 * gib);
  ByteLedger &first = host;
  ByteLedger &second = vram;
  CHECK(first.take(4 * gib));
  CHECK(!first.take(1u));
  CHECK(second.take(14 * gib));
  CHECK(!second.take(1u));
  first.give(gib);
  second.give(gib);
  CHECK_EQ(host.used(), 3 * gib);
  CHECK_EQ(vram.used(), 13 * gib);
  CHECK_EQ(first.free_bytes(), gib);
  CHECK_EQ(second.free_bytes(), gib); }

TEST(byte_ledger, a_refusal_is_counted_once_and_changes_nothing)
{ HostRamLedger ledger;
  ledger.budget(10u);
  ByteLedger &base = ledger;
  CHECK(base.take(10u));
  CHECK(!base.take(1u));
  CHECK(!base.take(UINT64_MAX));
  CHECK_EQ(base.refusals(), 2u);
  CHECK_EQ(base.used(), 10u); }

TEST(byte_ledger, a_cap_lowered_below_what_is_used_refuses_everything_until_enough_is_given)
{ HostRamLedger ledger;
  ledger.budget(100u);
  ByteLedger &base = ledger;
  CHECK(base.take(80u));
  ledger.budget(50u);
  CHECK(!base.take(1u));
  CHECK_EQ(base.free_bytes(), 0u);
  base.give(40u);
  CHECK(base.take(10u)); }
