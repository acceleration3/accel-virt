#include "executor/budgeted_heap.hpp"
#include "executor/host_ram_ledger.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::BudgetedHeap;
using av::HostRamLedger;
using av::Pool;

namespace {

constexpr std::uint64_t mib = 1ull << 20;

}

TEST(budgeted_heap, an_unbound_heap_hands_out_zeroed_blocks_and_counts_them)
{ BudgetedHeap heap;
  auto *const block = static_cast<std::uint8_t *>(heap.allocate(100u, Pool::paged));
  CHECK(block != nullptr);
  for (std::uint32_t at = 0u; at < 100u; at++) CHECK_EQ(block[at], 0u);
  CHECK_EQ(heap.live(), 1u);
  CHECK_EQ(heap.held(), 100u + BudgetedHeap::header_bytes);
  heap.release(block);
  CHECK_EQ(heap.live(), 0u);
  CHECK_EQ(heap.held(), 0u);
  heap.release(nullptr); }

TEST(budgeted_heap, blocks_are_sixteen_byte_aligned)
{ BudgetedHeap heap;
  void *const first = heap.allocate(1u, Pool::paged);
  void *const second = heap.allocate(24u, Pool::non_paged);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(first) % 16u, 0u);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(second) % 16u, 0u);
  heap.release(first);
  heap.release(second); }

TEST(budgeted_heap, a_session_limit_refuses_the_block_that_would_pass_it_and_frees_make_room)
{ BudgetedHeap heap;
  heap.bind(nullptr, 10u * mib);
  void *const first = heap.allocate(6u * mib, Pool::paged);
  CHECK(first != nullptr);
  CHECK(heap.allocate(5u * mib, Pool::paged) == nullptr);
  CHECK_EQ(heap.refusals(), 1u);
  heap.release(first);
  void *const again = heap.allocate(9u * mib, Pool::paged);
  CHECK(again != nullptr);
  heap.release(again);
  CHECK_EQ(heap.held(), 0u); }

TEST(budgeted_heap, the_vms_budget_is_shared_by_every_heap_on_the_ledger_and_comes_back_on_release)
{ HostRamLedger ledger;
  ledger.budget(10u * mib);
  BudgetedHeap first;
  BudgetedHeap second;
  first.bind(&ledger, 100u * mib);
  second.bind(&ledger, 100u * mib);
  void *const a = first.allocate(6u * mib, Pool::paged);
  CHECK(a != nullptr);
  CHECK(second.allocate(6u * mib, Pool::paged) == nullptr);
  CHECK_EQ(ledger.used(), 6u * mib + BudgetedHeap::header_bytes);
  first.release(a);
  CHECK_EQ(ledger.used(), 0u);
  void *const b = second.allocate(6u * mib, Pool::paged);
  CHECK(b != nullptr);
  second.release(b);
  CHECK_EQ(ledger.used(), 0u); }

TEST(budgeted_heap, a_size_that_wraps_with_the_header_is_refused)
{ BudgetedHeap heap;
  CHECK(heap.allocate(SIZE_MAX, Pool::paged) == nullptr);
  CHECK(heap.allocate(SIZE_MAX - BudgetedHeap::header_bytes + 1u, Pool::paged) == nullptr);
  CHECK_EQ(heap.live(), 0u); }

TEST(budgeted_heap, rebinding_to_another_ledger_returns_what_the_old_one_lent)
{ HostRamLedger old_ledger;
  HostRamLedger new_ledger;
  BudgetedHeap  heap;
  heap.bind(&old_ledger, 100u * mib);
  void *const block = heap.allocate(mib, Pool::paged);
  CHECK(old_ledger.used() != 0u);
  heap.bind(&new_ledger, 100u * mib);
  CHECK_EQ(old_ledger.used(), 0u);
  heap.release(block);
  CHECK_EQ(new_ledger.used(), 0u);
  CHECK_EQ(heap.limit(), 100u * mib); }
