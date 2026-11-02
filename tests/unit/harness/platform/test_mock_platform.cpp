#include "harness/virtio/fake_register_bytes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/service_value.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::FakeRegisterBytes;
using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::Pool;
using av::ServiceValue;
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

TEST(mock_platform_atomic, stores_and_loads_the_value)
{ MockPlatform platform;
  std::uint64_t word = 1u;
  platform.set_irql(Irql::isr);
  platform.store_atomic(word, 0x123456789ABCull);
  CHECK_EQ(platform.load_atomic(word), 0x123456789ABCull);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_atomic, a_load_hook_runs_before_each_load_reads_so_a_store_can_land_between_two)
{ MockPlatform platform;
  std::uint64_t word = 1u;
  struct Landing {
      MockPlatform  *platform;
      std::uint64_t *word;
      std::uint32_t  calls;
  } landing{&platform, &word, 0u};
  platform.on_load([](void *context) {
                      auto *const hook = static_cast<Landing *>(context);
                      if (++hook->calls == 2u) *hook->word = 7u;
                  },
                  &landing);
  CHECK_EQ(platform.load_atomic(word), 1u);
  CHECK_EQ(platform.load_atomic(word), 7u);
  CHECK_EQ(landing.calls, 2u);
  platform.on_load(nullptr, nullptr);
  CHECK_EQ(platform.load_atomic(word), 7u);
  CHECK_EQ(landing.calls, 2u); }

TEST(mock_platform_atomic, an_add_answers_the_sum_and_wraps_like_the_interlocked_one)
{ MockPlatform platform;
  std::uint64_t word = 5u;
  platform.set_irql(Irql::isr);
  CHECK_EQ(platform.add_atomic(word, 3u), 8u);
  CHECK_EQ(word, 8u);
  CHECK_EQ(platform.add_atomic(word, ~std::uint64_t{0}), 7u);   /* adding the all-ones is subtracting one */
  CHECK_EQ(platform.load_atomic(word), 7u);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

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
  CHECK_EQ(std::strlen(platform.last_log()), 0u);
  platform.log("a line");
  CHECK(std::strcmp(platform.last_log(), "a line") == 0);
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

/* A stall holds a processor and a wait does not: the two clocks are what a
 * test of a waiter judges how it waited by. */
TEST(mock_platform_stall, counts_the_ticks_it_held_the_processor_apart_from_sleep)
{ MockPlatform platform;
  platform.stall(30u);
  platform.stall(20u);
  CHECK_EQ(platform.stalled_ticks(), 500u);
  CHECK_EQ(platform.slept_ticks(), 0u);
  alignas(8) unsigned char event[32];
  platform.initialize_wake_event(event);
  CHECK(!platform.wait_wake_event(event, 7000u));
  CHECK_EQ(platform.slept_ticks(), 7000u);
  CHECK_EQ(platform.stalled_ticks(), 500u);
  CHECK_EQ(platform.interrupt_time(), 7500u); }

/* Only a wait that times out moves the clock or runs the device: a set event
 * is consumed at once. */
TEST(mock_platform_wake_event, a_set_event_is_taken_without_the_device_or_the_clock)
{ MockPlatform platform;
  std::uint32_t calls = 0u;
  platform.on_stall([](void *context) { ++*static_cast<std::uint32_t *>(context); }, &calls);
  alignas(8) unsigned char event[32];
  platform.initialize_wake_event(event);
  platform.set_wake_event(event);
  CHECK(platform.wait_wake_event(event, 9u));
  CHECK_EQ(calls, 0u);
  CHECK_EQ(platform.interrupt_time(), 0u);
  CHECK(!platform.wait_wake_event(event, 9u));
  CHECK_EQ(calls, 1u);
  CHECK_EQ(platform.interrupt_time(), 9u);
  platform.on_stall(nullptr, nullptr);
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

/* A frame is the kernel's to map only if it is guest memory: here, a page of a
 * contiguous block, whose own bytes the view reaches. */
TEST(mock_platform_map_frame, maps_a_page_of_a_contiguous_block_onto_its_bytes)
{ MockPlatform platform;
  auto *const block = static_cast<std::uint8_t *>(platform.allocate_contiguous(0x2000u));
  const std::uint64_t second = platform.physical_address(block) + 0x1000u;
  std::uint8_t *const view = platform.map_frame(second);
  CHECK(view == block + 0x1000u);
  CHECK_EQ(platform.mapped_frames(), 1u);
  CHECK(platform.map_frame(0x7000u) == nullptr);   /* no block holds it */
  platform.unmap_frame(view);
  CHECK_EQ(platform.mapped_frames(), 0u);
  platform.release_contiguous(block, 0x2000u); }

TEST(mock_platform_map_frame, refuses_misuse_and_fails_on_request)
{ MockPlatform platform;
  auto *const block = static_cast<std::uint8_t *>(platform.allocate_contiguous(0x1000u));
  const std::uint64_t frame = platform.physical_address(block);
  static_cast<void>(platform.map_frame(frame + 1u));
  CHECK(platform.claim(Violation::frame_misuse));
  std::uint8_t other = 0u;
  platform.unmap_frame(&other);
  CHECK(platform.claim(Violation::frame_misuse));
  platform.fail_frame_map_at(1u);
  CHECK(platform.map_frame(frame) == nullptr);
  std::uint8_t *const view = platform.map_frame(frame);
  CHECK(view != nullptr);
  platform.set_irql(av::Irql::dispatch);
  platform.unmap_frame(view);
  static_cast<void>(platform.map_frame(0x9000u));
  platform.set_irql(av::Irql::passive);
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  platform.release_contiguous(block, 0x1000u); }

/* A process view of device memory: at a user-mode address of its own, recorded
 * against what it maps until it is unmapped. */
TEST(mock_platform_map_to_process, gives_each_range_its_own_view_until_unmapped)
{ MockPlatform platform;
  std::uint64_t first_view = 0u, second_view = 0u;
  void *first = nullptr, *second = nullptr;
  CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x2000u, av::PageCaching::write_back, first_view, first), av::Status::ok);
  CHECK_STATUS(platform.map_to_process(0x4000002000ull, 0x1000u, av::PageCaching::write_back, second_view, second), av::Status::ok);
  CHECK(first != nullptr && second != nullptr && first != second);
  CHECK(first_view >= 0x00007FF000000000ull);
  CHECK(second_view >= first_view + 0x2000u);
  CHECK_EQ(platform.process_mappings(), 2u);
  CHECK(platform.maps_into_process(first_view, 0x4000000000ull, 0x2000u));
  CHECK(!platform.maps_into_process(first_view, 0x4000000000ull, 0x1000u));
  CHECK(!platform.maps_into_process(first_view, 0x4000001000ull, 0x2000u));
  CHECK(!platform.maps_into_process(first_view + 0x1000u, 0x4000000000ull, 0x2000u));
  platform.unmap_from_process(first);
  CHECK_EQ(platform.process_mappings(), 1u);
  CHECK(!platform.maps_into_process(first_view, 0x4000000000ull, 0x2000u));
  platform.unmap_from_process(second);
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

/* A view keeps the caching it was mapped with, so a test says a process sees
 * device memory the way the device said it must. */
TEST(mock_platform_map_to_process, counts_the_views_made_write_combined)
{ MockPlatform platform;
  std::uint64_t cached_view = 0u, combined_view = 0u;
  void *cached = nullptr, *combined = nullptr;
  CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x1000u, av::PageCaching::write_back, cached_view, cached),
               av::Status::ok);
  CHECK_EQ(platform.write_combined_process_mappings(), 0u);
  CHECK_STATUS(platform.map_to_process(0x4000001000ull, 0x1000u, av::PageCaching::write_combined, combined_view,
                                       combined),
               av::Status::ok);
  CHECK_EQ(platform.write_combined_process_mappings(), 1u);
  CHECK_EQ(platform.process_mappings(), 2u);
  platform.unmap_from_process(combined);
  CHECK_EQ(platform.write_combined_process_mappings(), 0u);
  platform.unmap_from_process(cached);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_map_to_process, refuses_misuse_and_fails_on_request)
{ MockPlatform platform;
  std::uint64_t view = 7u;
  void *mapping = &view;
  platform.fail_process_map_at(2u);
  CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x1000u, av::PageCaching::write_back, view, mapping), av::Status::ok);
  platform.unmap_from_process(mapping);
  CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x1000u, av::PageCaching::write_back, view, mapping), av::Status::no_memory);
  CHECK_EQ(view, 0u);
  CHECK(mapping == nullptr);
  CHECK_EQ(platform.process_mappings(), 0u);
  /* Not whole pages, in either the address or the size, or no pages at all. */
  const std::uint64_t misuses[][2] = {{0x4000000800ull, 0x1000u}, {0x4000000000ull, 0x800u}, {0x4000000000ull, 0u}};
  for (const auto &misuse : misuses) {
      CHECK_STATUS(platform.map_to_process(misuse[0], misuse[1], av::PageCaching::write_back, view, mapping), av::Status::ok);
      CHECK(platform.claim(Violation::frame_misuse));
      platform.unmap_from_process(mapping);
  }
  /* A mapping never made, and one already unmapped. */
  platform.unmap_from_process(&view);
  CHECK(platform.claim(Violation::frame_misuse));
  platform.unmap_from_process(mapping);
  CHECK(platform.claim(Violation::frame_misuse));
  platform.set_irql(Irql::apc);
  CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x1000u, av::PageCaching::write_back, view, mapping), av::Status::ok);
  platform.unmap_from_process(mapping);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

namespace {

/* A device that backs one page of its memory, at 0x4000001000. */
std::uint8_t backed_page[0x1000] = {};

void *backing_of(void *context, std::uint64_t physical, std::uint64_t)
{
    *static_cast<int *>(context) += 1;
    return physical == 0x4000001000ull ? backed_page : nullptr;
}

}  // namespace

/* A rig whose device backs its memory answers where a view really is; memory
 * it does not back still gets an address of its own. */
TEST(mock_platform_map_to_process, a_resolver_puts_the_view_on_the_memory_behind_it)
{ MockPlatform platform;
  int asked = 0;
  platform.resolve_process_views(&backing_of, &asked);
  std::uint64_t backed = 0u, fake = 0u;
  void *first = nullptr, *second = nullptr;
  CHECK_STATUS(platform.map_to_process(0x4000001000ull, 0x1000u, av::PageCaching::write_back, backed, first), av::Status::ok);
  CHECK_STATUS(platform.map_to_process(0x4000002000ull, 0x1000u, av::PageCaching::write_back, fake, second), av::Status::ok);
  CHECK_EQ(asked, 2);
  CHECK(backed == reinterpret_cast<std::uintptr_t>(backed_page));
  CHECK(fake >= 0x00007FF000000000ull);
  CHECK(platform.maps_into_process(backed, 0x4000001000ull, 0x1000u));
  platform.unmap_from_process(first);
  platform.unmap_from_process(second);
  platform.resolve_process_views(nullptr, nullptr);
  CHECK_STATUS(platform.map_to_process(0x4000001000ull, 0x1000u, av::PageCaching::write_back, backed, first), av::Status::ok);
  CHECK_EQ(asked, 2);
  CHECK(backed != reinterpret_cast<std::uintptr_t>(backed_page));
  platform.unmap_from_process(first); }

/* Pages the driver owns: zeroed, reachable by frame as a device reaches guest
 * memory, and one memory with the process's view of them. */
TEST(mock_platform_pages, are_zeroed_and_reached_by_frame_and_by_process_view)
{ MockPlatform platform;
  void *const pages = platform.allocate_pages(0x3000u);
  CHECK(pages != nullptr);
  CHECK_EQ(platform.owned_page_blocks(), 1u);
  std::uint64_t frames[3] = {};
  CHECK_STATUS(platform.read_page_frames(pages, 0u, 3u, frames), av::Status::ok);
  CHECK_EQ(frames[1], frames[0] + 0x1000u);
  CHECK_EQ(frames[2], frames[0] + 0x2000u);
  CHECK_EQ(frames[0] & 0xFFFu, 0u);
  /* The last two, and nothing past the end. */
  std::uint64_t tail[2] = {};
  CHECK_STATUS(platform.read_page_frames(pages, 1u, 2u, tail), av::Status::ok);
  CHECK_EQ(tail[0], frames[1]);
  CHECK_STATUS(platform.read_page_frames(pages, 2u, 2u, tail), av::Status::invalid_parameter);
  std::uint64_t view = 0u;
  void *mapping = nullptr;
  CHECK_STATUS(platform.map_pages_to_process(pages, view, mapping), av::Status::ok);
  CHECK(platform.maps_into_process(view, frames[0], 0x3000u));
  auto *const bytes = reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(view));
  CHECK_EQ(bytes[0], 0u);
  CHECK_EQ(bytes[0x2FFF], 0u);
  bytes[0x1004] = 0x5Au;
  std::uint8_t *const page = platform.map_frame(frames[1]);
  CHECK(page != nullptr);
  CHECK_EQ(page[4], 0x5Au);
  platform.unmap_frame(page);
  platform.unmap_from_process(mapping);
  platform.release_pages(pages);
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

/* Blocks are numbered in the order they were handed out - a refused one takes
 * no number - and a test reaches one by its number only while it is live. */
TEST(mock_platform_pages, are_reached_by_the_order_they_were_made_while_live)
{ MockPlatform platform;
  CHECK_EQ(platform.page_blocks_made(), 0u);
  CHECK(platform.page_block(0u) == nullptr);
  void *const first = platform.allocate_pages(0x1000u);
  platform.fail_page_allocation_at(1u);
  CHECK(platform.allocate_pages(0x1000u) == nullptr);
  void *const second = platform.allocate_pages(0x2000u);
  CHECK_EQ(platform.page_blocks_made(), 2u);
  std::uint64_t frame = 0u;
  CHECK_STATUS(platform.read_page_frames(second, 1u, 1u, &frame), av::Status::ok);
  std::uint8_t *const page = platform.map_frame(frame);
  page[7] = 0x3Cu;
  CHECK_EQ(platform.page_block(1u)[0x1007], 0x3Cu);
  CHECK(platform.page_block(0u) != nullptr && platform.page_block(0u) != platform.page_block(1u));
  CHECK(platform.page_block(2u) == nullptr);
  platform.unmap_frame(page);
  platform.release_pages(first);
  CHECK(platform.page_block(0u) == nullptr);
  void *const third = platform.allocate_pages(0x1000u);   /* the freed slot, a new number */
  CHECK(platform.page_block(0u) == nullptr);
  CHECK(platform.page_block(2u) != nullptr);
  platform.release_pages(second);
  platform.release_pages(third);
  CHECK_EQ(platform.page_blocks_made(), 3u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_pages, refuse_partial_pages_dispatch_and_the_requested_failure)
{ MockPlatform platform;
  CHECK(platform.allocate_pages(0u) == nullptr);
  CHECK(platform.allocate_pages(0x1800u) == nullptr);
  platform.set_irql(Irql::dispatch);
  CHECK(platform.allocate_pages(0x1000u) == nullptr);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  platform.fail_page_allocation_at(2u);
  void *const first = platform.allocate_pages(0x1000u);
  CHECK(first != nullptr);
  CHECK(platform.allocate_pages(0x1000u) == nullptr);
  void *const third = platform.allocate_pages(0x1000u);
  CHECK(third != nullptr);
  /* A failed process map leaves nothing mapped, and counts with map_to_process. */
  std::uint64_t view = 9u;
  void *mapping = &view;
  platform.fail_process_map_at(1u);
  CHECK_STATUS(platform.map_pages_to_process(first, view, mapping), av::Status::no_memory);
  CHECK_EQ(view, 0u);
  CHECK(mapping == nullptr);
  CHECK_EQ(platform.process_mappings(), 0u);
  platform.release_pages(first);
  platform.release_pages(third);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

/* Pages freed under a process's view would be memory it writes after someone
 * else owns it. */
TEST(mock_platform_pages, releasing_pages_a_process_still_maps_is_misuse)
{ MockPlatform platform;
  void *const pages = platform.allocate_pages(0x1000u);
  std::uint64_t view = 0u;
  void *mapping = nullptr;
  CHECK_STATUS(platform.map_pages_to_process(pages, view, mapping), av::Status::ok);
  platform.release_pages(pages);
  CHECK(platform.claim(Violation::frame_misuse));
  platform.unmap_from_process(mapping);
  /* A record never allocated maps nothing. */
  int stranger = 0;
  CHECK_STATUS(platform.map_pages_to_process(&stranger, view, mapping), av::Status::no_memory);
  CHECK(platform.claim(Violation::frame_misuse));
  CHECK_STATUS(platform.read_page_frames(&stranger, 0u, 1u, &view), av::Status::invalid_parameter);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mock_platform_destruction, pages_left_allocated_fail_the_test)
{ { MockPlatform platform;
    CHECK(platform.allocate_pages(0x1000u) != nullptr); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(mock_platform_destruction, a_process_view_left_mapped_fails_the_test)
{ { MockPlatform platform;
    std::uint64_t view = 0u;
    void *mapping = nullptr;
    CHECK_STATUS(platform.map_to_process(0x4000000000ull, 0x1000u, av::PageCaching::write_back, view, mapping), av::Status::ok); }
  CHECK(TestRegistry::instance().take_current_failure()); }

/* DriverEntry reads its service key through the platform, which a test builds
 * with the one value it wants there: the name must match, and anything else
 * reads as the caller's fallback, as a missing registry value does. */
TEST(mock_platform_service, answers_its_one_value_and_falls_back_for_the_rest)
{ int registry_path = 0;
  const ServiceValue scopes("TraceScopes", 7u);
  MockPlatform platform(0u, scopes);
  CHECK_EQ(platform.read_service_u32(&registry_path, "TraceScopes", 3u), 7u);
  CHECK_EQ(platform.read_service_u32(&registry_path, "TraceScope", 3u), 3u);
  CHECK_EQ(platform.read_service_u32(&registry_path, "Other", 9u), 9u);
  MockPlatform plain;
  CHECK_EQ(plain.read_service_u32(&registry_path, "TraceScopes", 3u), 3u); }
