#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::Pool;
using av::TestRegistry;
using av::Violation;

namespace {

constexpr LockRank low{10};
constexpr LockRank high{20};

class MutexStorage {
public:
    void *bytes() { return bytes_; }

private:
    unsigned char bytes_[64] = {};
};

}  // namespace

/* ---- each rule, provoked and claimed ------------------------------------ */

TEST(mock_platform_irql, a_call_above_its_ceiling_is_recorded)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  platform.require_irql_at_most(Irql::passive, "probe");
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

TEST(mock_platform_irql, a_call_at_its_ceiling_is_legal)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  platform.require_irql_at_most(Irql::dispatch, "probe");
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_allocate, refuses_at_dispatch)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  CHECK(platform.allocate(16u, Pool::non_paged) == nullptr);
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(mock_platform_allocate, zero_bytes_is_an_error_not_a_violation)
{ MockPlatform platform;
  CHECK(platform.allocate(0u, Pool::paged) == nullptr);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_allocate, fails_exactly_the_requested_allocation)
{ MockPlatform platform;
  platform.fail_allocation_at(2u);
  void *first = platform.allocate(8u, Pool::non_paged);
  CHECK(first != nullptr);
  CHECK(platform.allocate(8u, Pool::non_paged) == nullptr);
  void *third = platform.allocate(8u, Pool::non_paged);
  CHECK(third != nullptr);
  CHECK_EQ(platform.allocations_made(), 3u);
  platform.release(first);
  platform.release(third); }

TEST(mock_platform_allocate, a_failure_point_past_the_last_allocation_never_fires)
{ MockPlatform platform;
  platform.fail_allocation_at(5u);
  void *block = platform.allocate(8u, Pool::non_paged);
  CHECK(block != nullptr);
  platform.release(block);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(mock_platform_release, a_null_pointer_is_recorded)
{ MockPlatform platform;
  platform.release(nullptr);
  CHECK(platform.claim(Violation::null_release)); }

TEST(mock_platform_mutex, acquiring_at_dispatch_is_blocking_at_dispatch)
{ MockPlatform platform;
  MutexStorage storage;
  platform.initialize_mutex(storage.bytes());
  platform.set_irql(Irql::dispatch);
  platform.acquire_mutex(storage.bytes(), low);
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  platform.release_mutex(storage.bytes(), low);
  CHECK(platform.current_irql() == Irql::dispatch); }

TEST(mock_platform_mutex, raises_to_apc_and_restores)
{ MockPlatform platform;
  MutexStorage storage;
  platform.initialize_mutex(storage.bytes());
  platform.acquire_mutex(storage.bytes(), low);
  CHECK(platform.current_irql() == Irql::apc);
  CHECK_EQ(platform.locks_held(), 1u);
  platform.release_mutex(storage.bytes(), low);
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(mock_platform_spin_lock, raises_to_dispatch_and_returns_the_level_to_restore)
{ MockPlatform platform;
  std::uintptr_t word = 0u;
  const Irql previous = platform.acquire_spin_lock(word, low);
  CHECK(previous == Irql::passive);
  CHECK(platform.current_irql() == Irql::dispatch);
  platform.release_spin_lock(word, low, previous);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(mock_platform_spin_lock, never_lowers_an_irql_above_dispatch)
{ MockPlatform platform;
  std::uintptr_t word = 0u;
  platform.set_irql(Irql::isr);
  const Irql previous = platform.acquire_spin_lock(word, low);
  CHECK(platform.current_irql() == Irql::isr);
  platform.release_spin_lock(word, low, previous);
  CHECK(platform.current_irql() == Irql::isr); }

TEST(mock_platform_lock_order, increasing_ranks_are_legal)
{ MockPlatform platform;
  std::uintptr_t a = 0u, b = 0u;
  const Irql first = platform.acquire_spin_lock(a, low);
  const Irql second = platform.acquire_spin_lock(b, high);
  platform.release_spin_lock(b, high, second);
  platform.release_spin_lock(a, low, first);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_lock_order, a_lower_rank_under_a_higher_one_is_recorded)
{ MockPlatform platform;
  std::uintptr_t a = 0u, b = 0u;
  const Irql first = platform.acquire_spin_lock(a, high);
  const Irql second = platform.acquire_spin_lock(b, low);
  CHECK(platform.claim(Violation::lock_order));
  platform.release_spin_lock(b, low, second);
  platform.release_spin_lock(a, high, first);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_lock_order, an_equal_rank_is_recorded)
{ MockPlatform platform;
  MutexStorage a, b;
  platform.initialize_mutex(a.bytes());
  platform.initialize_mutex(b.bytes());
  platform.acquire_mutex(a.bytes(), low);
  platform.acquire_mutex(b.bytes(), low);
  CHECK(platform.claim(Violation::lock_order));
  platform.release_mutex(b.bytes(), low);
  platform.release_mutex(a.bytes(), low); }

TEST(mock_platform_release_order, releasing_below_the_top_is_recorded)
{ MockPlatform platform;
  std::uintptr_t a = 0u, b = 0u;
  const Irql first = platform.acquire_spin_lock(a, low);
  const Irql second = platform.acquire_spin_lock(b, high);
  platform.release_spin_lock(a, low, first);
  CHECK(platform.claim(Violation::release_order));
  platform.release_spin_lock(b, high, second);
  CHECK_EQ(platform.locks_held(), 1u);
  platform.release_spin_lock(a, low, first);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(mock_platform_release_order, releasing_a_lock_not_held_is_recorded)
{ MockPlatform platform;
  std::uintptr_t word = 0u;
  platform.release_spin_lock(word, low, Irql::passive);
  CHECK(platform.claim(Violation::release_order)); }

TEST(mock_platform_lock_stack, overflow_is_recorded_and_the_lock_still_taken)
{ MockPlatform platform;
  std::uintptr_t words[MockPlatform::lock_stack_depth + 1u] = {};
  Irql saved[MockPlatform::lock_stack_depth + 1u] = {};
  for (std::uint16_t i = 0u; i <= MockPlatform::lock_stack_depth; i++)
      saved[i] = platform.acquire_spin_lock(words[i], LockRank{static_cast<std::uint16_t>(i + 1u)});
  CHECK(platform.claim(Violation::lock_stack_overflow));
  CHECK(platform.current_irql() == Irql::dispatch);
  CHECK_EQ(platform.locks_held(), MockPlatform::lock_stack_depth);
  platform.release_spin_lock(words[MockPlatform::lock_stack_depth],
                             LockRank{static_cast<std::uint16_t>(MockPlatform::lock_stack_depth + 1u)},
                             saved[MockPlatform::lock_stack_depth]);
  CHECK(platform.claim(Violation::release_order));
  for (std::uint16_t i = MockPlatform::lock_stack_depth; i > 0u; i--)
      platform.release_spin_lock(words[i - 1u], LockRank{i}, saved[i - 1u]);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(mock_platform_invariant, a_broken_invariant_is_recorded)
{ MockPlatform platform;
  platform.report_broken_invariant("probe_invariant", "probe");
  CHECK(platform.claim(Violation::broken_invariant)); }

/* ---- time --------------------------------------------------------------- */

TEST(mock_platform_time, stands_still_until_advanced)
{ MockPlatform platform;
  CHECK_EQ(platform.interrupt_time(), platform.interrupt_time()); }

TEST(mock_platform_time, advances_by_exactly_the_requested_ticks)
{ MockPlatform platform;
  const std::uint64_t before = platform.interrupt_time();
  platform.advance_time(20000000u);
  CHECK(platform.interrupt_time() - before == 20000000u); }

TEST(mock_platform_time, saturates_instead_of_wrapping)
{ MockPlatform platform;
  platform.advance_time(~std::uint64_t{0});
  platform.advance_time(1u);
  CHECK(platform.interrupt_time() == ~std::uint64_t{0});
  CHECK(platform.claim(Violation::time_overflow)); }

/* ---- a mock that is destroyed dirty fails the test ----------------------- */

TEST(mock_platform_destruction, an_unclaimed_violation_fails_the_test)
{ { MockPlatform platform;
    platform.release(nullptr); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_destruction, a_leak_fails_the_test)
{ void *leaked = nullptr;
  { MockPlatform platform;
    leaked = platform.allocate(8u, Pool::paged);
    CHECK(leaked != nullptr); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_destruction, a_held_lock_fails_the_test)
{ { MockPlatform platform;
    std::uintptr_t word = 0u;
    (void)platform.acquire_spin_lock(word, low); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_destruction, a_clean_mock_does_not_fail_the_test)
{ { MockPlatform platform;
    platform.release(nullptr);
    CHECK(platform.claim(Violation::null_release)); }
  CHECK(!TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_log, logging_is_not_a_violation)
{ MockPlatform platform;
  platform.log("a line");
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_memory, zero_and_copy_touch_only_their_bytes)
{ MockPlatform platform;
  unsigned char bytes[4] = {1, 2, 3, 4};
  unsigned char copy[4] = {9, 9, 9, 9};
  platform.zero(bytes + 1, 2u);
  platform.copy(copy, bytes, 3u);
  CHECK_EQ(copy[0], 1); CHECK_EQ(copy[1], 0); CHECK_EQ(copy[2], 0); CHECK_EQ(copy[3], 9); }
