#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/vram_policy.hpp"

#include <cstdint>
#include <thread>

using av::VramLedger;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

}

TEST(vram_ledger, a_ledger_that_has_learnt_no_card_answers_to_nobody)
{ VramLedger ledger;
  CHECK(ledger.take(100 * gib));
  CHECK_EQ(ledger.used(), 100 * gib);
  CHECK_EQ(ledger.refusals(), 0u);
  CHECK(ledger.invariants_hold()); }

TEST(vram_ledger, the_cap_is_the_policys_cap_of_the_card_and_follows_the_reserve)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK_EQ(ledger.total(), 16 * gib);
  CHECK_EQ(ledger.cap(), 14 * gib);
  ledger.reserve_vram(4096u);
  CHECK_EQ(ledger.cap(), 12 * gib);
  CHECK(ledger.invariants_hold()); }

TEST(vram_ledger, a_take_that_would_pass_the_cap_is_refused_and_changes_nothing)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK(ledger.take(10 * gib));
  CHECK(!ledger.take(5 * gib));
  CHECK_EQ(ledger.used(), 10 * gib);
  CHECK_EQ(ledger.refusals(), 1u);
  CHECK_EQ(ledger.free_bytes(), 4 * gib); }

TEST(vram_ledger, the_cap_itself_fits_and_one_byte_more_does_not)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK(ledger.take(14 * gib));
  CHECK_EQ(ledger.free_bytes(), 0u);
  CHECK(!ledger.take(1u));
  ledger.give(14 * gib);
  CHECK(!ledger.take(14 * gib + 1u));
  CHECK(ledger.take(14 * gib)); }

TEST(vram_ledger, a_huge_take_cannot_wrap_the_sum_past_the_cap)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK(ledger.take(1u));
  CHECK(!ledger.take(UINT64_MAX));
  CHECK(!ledger.take(UINT64_MAX - 1u));
  CHECK_EQ(ledger.used(), 1u); }

TEST(vram_ledger, giving_back_more_than_is_held_leaves_zero_not_a_wrapped_count)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK(ledger.take(gib));
  ledger.give(5 * gib);
  CHECK_EQ(ledger.used(), 0u); }

TEST(vram_ledger, lowering_the_cap_below_what_is_held_refuses_new_takes_and_keeps_the_old)
{ VramLedger ledger;
  ledger.learn(16 * gib);
  CHECK(ledger.take(13 * gib));
  ledger.reserve_vram(8192u);
  CHECK_EQ(ledger.cap(), 8 * gib);
  CHECK(!ledger.take(1u));
  CHECK_EQ(ledger.used(), 13 * gib);
  CHECK_EQ(ledger.free_bytes(), 0u); }

TEST(vram_ledger, racing_threads_never_hold_more_than_the_cap_and_end_at_zero)
{ VramLedger ledger;
  ledger.learn(2 * gib);
  const std::uint64_t cap = ledger.cap();
  std::uint64_t       over = 0u;
  auto work = [&] {
      for (std::uint32_t i = 0u; i < 20000u; i++) {
          if (!ledger.take(cap / 3u)) continue;
          if (ledger.used() > cap) __atomic_add_fetch(&over, 1u, __ATOMIC_RELAXED);
          ledger.give(cap / 3u);
      }
  };
  std::thread a(work), b(work), c(work), d(work);
  a.join(); b.join(); c.join(); d.join();
  CHECK_EQ(over, 0u);
  CHECK_EQ(ledger.used(), 0u); }
