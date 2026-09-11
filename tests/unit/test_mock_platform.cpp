#include "../harness/fake_register_bytes.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::FakeRegisterBytes;
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

TEST(mock_platform_time, the_performance_counter_is_the_test_clock)
{ MockPlatform platform;
  platform.advance_time(1234u);
  std::uint64_t frequency = 0u;
  CHECK_EQ(platform.performance_counter(frequency), 1234u);
  CHECK_EQ(frequency, 10000000u); }

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

/* ---- contiguous DMA memory and stalls -------------------------------------- */

TEST(mock_platform_contiguous, blocks_get_distinct_page_aligned_physical_addresses)
{ MockPlatform platform;
  void *first = platform.allocate_contiguous(100u);
  void *second = platform.allocate_contiguous(8192u);
  CHECK(first != nullptr); CHECK(second != nullptr);
  const std::uint64_t first_address = platform.physical_address(first);
  const std::uint64_t second_address = platform.physical_address(second);
  CHECK(first_address != 0u); CHECK((first_address & 0xFFFu) == 0u);
  CHECK((second_address & 0xFFFu) == 0u);
  CHECK(second_address >= first_address + 4096u);
  CHECK_EQ(platform.live_allocations(), 2u);
  platform.release_contiguous(first, 100u);
  platform.release_contiguous(second, 8192u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(mock_platform_contiguous, translates_addresses_inside_a_block_only)
{ MockPlatform platform;
  auto *block = static_cast<std::uint8_t *>(platform.allocate_contiguous(64u));
  const std::uint64_t address = platform.physical_address(block);
  CHECK(platform.translate_physical(address, 64u) == block);
  CHECK(platform.translate_physical(address + 10u, 4u) == block + 10);
  CHECK(platform.translate_physical(address + 60u, 5u) == nullptr);
  CHECK(platform.translate_physical(address - 1u, 1u) == nullptr);
  CHECK(platform.translate_physical(address, ~std::size_t{0}) == nullptr);
  CHECK(platform.translate_physical(0u, 1u) == nullptr);
  platform.release_contiguous(block, 64u); }

TEST(mock_platform_contiguous, refuses_at_dispatch)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  CHECK(platform.allocate_contiguous(64u) == nullptr);
  CHECK(platform.claim(Violation::allocation_at_dispatch)); }

TEST(mock_platform_contiguous, honours_the_failure_point_and_zero_bytes)
{ MockPlatform platform;
  CHECK(platform.allocate_contiguous(0u) == nullptr);
  platform.fail_allocation_at(1u);
  CHECK(platform.allocate_contiguous(64u) == nullptr);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_contiguous, a_leaked_block_fails_the_test)
{ { MockPlatform platform;
    CHECK(platform.allocate_contiguous(64u) != nullptr); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_stall, is_legal_at_dispatch_and_advances_the_clock)
{ MockPlatform platform;
  platform.set_irql(Irql::dispatch);
  const std::uint64_t before = platform.interrupt_time();
  platform.stall(1000u);
  CHECK(platform.interrupt_time() - before == 10000u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_register, routes_each_width_to_the_attached_window)
{ MockPlatform platform; FakeRegisterBytes window(16u);
  const auto base = reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x7F0000001000u});
  platform.attach_register_window(base, window);
  platform.write_register_u8(base + 1u, 0xABu);
  CHECK_EQ(window.last_write_offset(), 1u);
  platform.write_register_u16(base + 2u, 0x1234u);
  platform.write_register_u32(base + 4u, 0xCAFEF00Du);
  platform.write_register_u64(base + 8u, 0x1122334455667788ull);
  CHECK_EQ(window.writes(), 4u);
  CHECK_EQ(platform.read_register_u8(base + 1u), 0xABu);
  CHECK_EQ(platform.read_register_u16(base + 2u), 0x1234u);
  CHECK_EQ(platform.read_register_u32(base + 4u), 0xCAFEF00Du);
  CHECK_EQ(platform.read_register_u32(base + 12u), 0x11223344u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_register, legal_at_any_irql)
{ MockPlatform platform; FakeRegisterBytes window(4u);
  const auto base = reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x7F0000002000u});
  platform.attach_register_window(base, window);
  platform.set_irql(Irql::isr);
  CHECK_EQ(platform.read_register_u8(base), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_register, an_access_outside_every_window_is_recorded)
{ MockPlatform platform; FakeRegisterBytes window(4u);
  const auto base = reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x7F0000003000u});
  platform.attach_register_window(base, window);
  CHECK_EQ(platform.read_register_u32(base + 1u), 0xFFFFFFFFu);   /* straddles the end */
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK_EQ(platform.read_register_u8(base - 1u), 0xFFu);
  CHECK(platform.claim(Violation::unmapped_register));
  platform.write_register_u16(base + 4u, 1u);
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK_EQ(platform.read_register_u16(base + 8u), 0xFFFFu);
  CHECK(platform.claim(Violation::unmapped_register));
  platform.write_register_u8(base + 4u, 1u);
  platform.write_register_u32(base + 4u, 1u);
  platform.write_register_u64(base + 4u, 1u);
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK_EQ(window.writes(), 0u); }

TEST(mock_platform_register, a_detached_window_is_unmapped)
{ MockPlatform platform; FakeRegisterBytes first(4u), second(4u);
  const auto base = reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x7F0000004000u});
  platform.attach_register_window(base, first);
  platform.attach_register_window(base + 4u, second);
  CHECK_EQ(platform.attached_register_windows(), 2u);
  platform.detach_register_windows(base, 8u);
  CHECK_EQ(platform.attached_register_windows(), 0u);
  CHECK_EQ(platform.read_register_u8(base + 4u), 0xFFu);
  CHECK(platform.claim(Violation::unmapped_register)); }

TEST(mock_platform_register, attaching_past_capacity_fails_the_test)
{ { MockPlatform platform; FakeRegisterBytes window(4u);
    const std::uintptr_t first = 0x7F0000010000u;
    for (std::uint32_t i = 0u; i <= MockPlatform::max_register_windows; i++)
        platform.attach_register_window(reinterpret_cast<std::uint8_t *>(first + 0x100u * i), window);
    CHECK_EQ(platform.attached_register_windows(), MockPlatform::max_register_windows); }
  CHECK(TestRegistry::instance().take_current_failure()); }

/* ---- timers --------------------------------------------------------------- */

namespace {

struct Ticks {
    void         *timer = nullptr;
    unsigned      count = 0u;
};

void count_tick(void *timer, void *context)
{
    Ticks &ticks = *static_cast<Ticks *>(context);
    ticks.timer = timer;
    ticks.count++;
}

}  // namespace

TEST(mock_platform_timer, fires_every_period_at_dispatch_once_started)
{ MockPlatform platform;
  Ticks ticks;
  void *timer = platform.create_timer(&count_tick, &ticks);
  CHECK(timer != nullptr);
  platform.run_timers(1000u);
  CHECK_EQ(ticks.count, 0u);
  platform.start_timer(timer, 100u);
  platform.run_timers(99u);
  CHECK_EQ(ticks.count, 0u);
  platform.run_timers(1u);
  CHECK_EQ(ticks.count, 1u);
  CHECK(ticks.timer == timer);
  platform.run_timers(250u);
  CHECK_EQ(ticks.count, 3u);
  CHECK_EQ(platform.interrupt_time(), 1350u);
  CHECK(platform.current_irql() == Irql::passive);
  platform.stop_timer(timer);
  platform.run_timers(1000u);
  CHECK_EQ(ticks.count, 3u);
  platform.destroy_timer(timer); }

TEST(mock_platform_timer, the_callback_runs_at_dispatch)
{ MockPlatform platform;
  struct Probe { MockPlatform *platform; Irql seen; } probe = {&platform, Irql::passive};
  void *timer = platform.create_timer(
      +[](void *, void *context) { auto *p = static_cast<Probe *>(context); p->seen = p->platform->current_irql(); },
      &probe);
  platform.start_timer(timer, 10u);
  platform.run_timers(10u);
  CHECK(probe.seen == Irql::dispatch);
  platform.destroy_timer(timer); }

TEST(mock_platform_timer, restarting_replaces_the_schedule)
{ MockPlatform platform;
  Ticks ticks;
  void *timer = platform.create_timer(&count_tick, &ticks);
  platform.start_timer(timer, 100u);
  platform.run_timers(50u);
  platform.start_timer(timer, 100u);
  platform.run_timers(99u);
  CHECK_EQ(ticks.count, 0u);
  platform.run_timers(1u);
  CHECK_EQ(ticks.count, 1u);
  platform.destroy_timer(timer); }

TEST(mock_platform_timer, two_timers_fire_in_due_order)
{ MockPlatform platform;
  Ticks slow, fast;
  void *slow_timer = platform.create_timer(&count_tick, &slow);
  void *fast_timer = platform.create_timer(&count_tick, &fast);
  platform.start_timer(slow_timer, 30u);
  platform.start_timer(fast_timer, 10u);
  platform.run_timers(30u);
  CHECK_EQ(slow.count, 1u);
  CHECK_EQ(fast.count, 3u);
  platform.destroy_timer(fast_timer);
  platform.destroy_timer(slow_timer); }

TEST(mock_platform_timer, creation_is_an_allocation)
{ MockPlatform platform;
  Ticks ticks;
  platform.fail_allocation_at(1u);
  CHECK(platform.create_timer(&count_tick, &ticks) == nullptr);
  platform.set_irql(Irql::dispatch);
  CHECK(platform.create_timer(&count_tick, &ticks) == nullptr);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  void *timers[MockPlatform::max_timers + 1u] = {};
  for (void *&timer : timers) timer = platform.create_timer(&count_tick, &ticks);
  CHECK(timers[MockPlatform::max_timers] == nullptr);
  for (unsigned index = 0u; index < MockPlatform::max_timers; index++) platform.destroy_timer(timers[index]); }

TEST(mock_platform_timer, start_and_stop_are_legal_at_dispatch_only_below_it_destroy)
{ MockPlatform platform;
  Ticks ticks;
  void *timer = platform.create_timer(&count_tick, &ticks);
  platform.set_irql(Irql::dispatch);
  platform.start_timer(timer, 10u);
  platform.stop_timer(timer);
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::isr);
  platform.start_timer(timer, 10u);
  platform.stop_timer(timer);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::dispatch);
  platform.destroy_timer(timer);
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  platform.set_irql(Irql::passive); }

TEST(mock_platform_timer, a_timer_it_did_not_create_or_a_zero_period_is_misuse)
{ MockPlatform platform;
  Ticks ticks;
  int stranger = 0;
  platform.start_timer(&stranger, 10u);
  platform.stop_timer(&stranger);
  platform.destroy_timer(&stranger);
  platform.destroy_timer(nullptr);
  for (int count = 0; count < 3; count++) CHECK(platform.claim(Violation::timer_misuse));
  CHECK(platform.claim(Violation::null_release));
  void *timer = platform.create_timer(&count_tick, &ticks);
  platform.start_timer(timer, 0u);
  CHECK(platform.claim(Violation::timer_misuse));
  platform.destroy_timer(timer);
  platform.destroy_timer(timer);
  CHECK(platform.claim(Violation::timer_misuse)); }

TEST(mock_platform_destruction, a_leaked_timer_fails_the_test)
{ Ticks ticks;
  { MockPlatform platform;
    static_cast<void>(platform.create_timer(&count_tick, &ticks)); }
  CHECK(TestRegistry::instance().take_current_failure()); }
