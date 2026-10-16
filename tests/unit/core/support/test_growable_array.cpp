#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_size(GrowableArray<T> &array, std::uint32_t size) { array.size_ = size; }

    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

}  // namespace av

using av::GrowableArray;
using av::GrowableArrayProbe;
using av::Irql;
using av::MockPlatform;
using av::Pool;
using av::Status;
using av::Violation;

namespace {

struct Range {
    std::uint64_t begin;
    std::uint64_t size;
};

struct Huge {
    unsigned char bytes[std::size_t{1} << 40];
};

bool holds_in_order(const GrowableArray<std::uint32_t> &array, std::uint32_t first, std::uint32_t count)
{
    if (array.size() != count) return false;
    for (std::uint32_t index = 0u; index < count; index++)
        if (array.at(index) != first + index) return false;
    return true;
}

}  // namespace

TEST(growable_array_append, starts_with_no_block_and_takes_one_at_the_first_value)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK(array.invariants_hold());
  CHECK_EQ(array.capacity(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(array.append(41u), Status::ok);
  CHECK(array.invariants_hold());
  CHECK_EQ(array.size(), 1u);
  CHECK_EQ(array.at(0u), 41u);
  CHECK_EQ(array.capacity(), GrowableArray<std::uint32_t>::first_capacity);
  CHECK_EQ(platform.live_allocations(), 1u); }

TEST(growable_array_append, doubles_past_each_block_keeping_every_value)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::non_paged);
  for (std::uint32_t value = 0u; value < 300000u; value++) {
      CHECK_STATUS(array.append(value), Status::ok);
      if ((value & (value + 1u)) == 0u) CHECK(array.invariants_hold());
  }
  CHECK(holds_in_order(array, 0u, 300000u));
  CHECK_EQ(array.capacity(), 524288u);
  CHECK_EQ(platform.live_allocations(), 1u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_append, leaves_everything_as_it_was_when_the_block_cannot_grow)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 0u; value < GrowableArray<std::uint32_t>::first_capacity; value++)
      CHECK_STATUS(array.append(value), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(array.append(99u), Status::no_memory);
  CHECK(array.invariants_hold());
  CHECK(holds_in_order(array, 0u, GrowableArray<std::uint32_t>::first_capacity));
  CHECK_EQ(array.capacity(), GrowableArray<std::uint32_t>::first_capacity);
  CHECK_STATUS(array.append(8u), Status::ok);
  CHECK(holds_in_order(array, 0u, 9u)); }

TEST(growable_array_append, the_first_block_failing_leaves_an_empty_array)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(array.append(1u), Status::no_memory);
  CHECK(array.invariants_hold());
  CHECK_EQ(array.size(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(growable_array_append, is_legal_up_to_apc)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(array.append(1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::allocation_at_dispatch)) {}
  array.clear(); }

TEST(growable_array_insert_at, shifts_the_tail_along_and_keeps_order)
{ MockPlatform platform;
  GrowableArray<Range> array(platform, Pool::paged);
  CHECK_STATUS(array.insert_at(0u, Range{0x3000u, 1u}), Status::ok);
  CHECK_STATUS(array.insert_at(0u, Range{0x1000u, 1u}), Status::ok);
  CHECK_STATUS(array.insert_at(1u, Range{0x2000u, 1u}), Status::ok);
  CHECK_STATUS(array.insert_at(3u, Range{0x4000u, 1u}), Status::ok);
  CHECK(array.invariants_hold());
  CHECK_EQ(array.size(), 4u);
  for (std::uint32_t index = 0u; index < 4u; index++) CHECK_EQ(array.at(index).begin, 0x1000u * (index + 1u)); }

TEST(growable_array_insert_at, refuses_an_index_past_the_end)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK_STATUS(array.insert_at(1u, 5u), Status::invalid_parameter);
  CHECK_STATUS(array.append(5u), Status::ok);
  CHECK_STATUS(array.insert_at(2u, 6u), Status::invalid_parameter);
  CHECK_STATUS(array.insert_at(0xFFFFFFFFu, 6u), Status::invalid_parameter);
  CHECK(holds_in_order(array, 5u, 1u));
  CHECK(array.invariants_hold()); }

TEST(growable_array_insert_at, grows_across_a_block_and_reports_a_failure_unchanged)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 1u; value <= GrowableArray<std::uint32_t>::first_capacity; value++)
      CHECK_STATUS(array.append(value), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(array.insert_at(0u, 0u), Status::no_memory);
  CHECK(holds_in_order(array, 1u, GrowableArray<std::uint32_t>::first_capacity));
  CHECK_STATUS(array.insert_at(0u, 0u), Status::ok);
  CHECK(holds_in_order(array, 0u, GrowableArray<std::uint32_t>::first_capacity + 1u));
  CHECK(array.invariants_hold()); }

TEST(growable_array_reserve, makes_room_so_appends_up_to_it_allocate_nothing)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK_STATUS(array.reserve(100u), Status::ok);
  CHECK_EQ(array.capacity(), 128u);
  CHECK_EQ(array.size(), 0u);
  platform.fail_allocation_at(1u);
  for (std::uint32_t value = 0u; value < 128u; value++) CHECK_STATUS(array.append(value), Status::ok);
  CHECK_STATUS(array.append(128u), Status::no_memory);
  CHECK(holds_in_order(array, 0u, 128u));
  CHECK_STATUS(array.reserve(4u), Status::ok);
  CHECK_EQ(array.capacity(), 128u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_reserve, refuses_more_than_a_block_may_hold)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK_STATUS(array.reserve(GrowableArray<std::uint32_t>::most_values + 1u), Status::no_memory);
  CHECK_STATUS(array.reserve(0xFFFFFFFFu), Status::no_memory);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_reserve, bounds_a_block_by_what_its_bytes_can_count)
{ CHECK_EQ(GrowableArray<std::uint32_t>::most_values, 0x80000000u);
  CHECK_EQ(GrowableArray<Huge>::most_values, 1u << 23u);
  MockPlatform platform;
  GrowableArray<Huge> array(platform, Pool::paged);
  CHECK_STATUS(array.reserve((1u << 23u) + 1u), Status::no_memory);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(growable_array_resize, fills_what_it_adds_and_drops_what_it_cuts)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK_STATUS(array.append(1u), Status::ok);
  CHECK_STATUS(array.resize(20u, 7u), Status::ok);
  CHECK_EQ(array.size(), 20u);
  CHECK_EQ(array.at(0u), 1u);
  for (std::uint32_t index = 1u; index < 20u; index++) CHECK_EQ(array.at(index), 7u);
  CHECK_STATUS(array.resize(2u, 9u), Status::ok);
  CHECK_EQ(array.size(), 2u);
  CHECK_EQ(array.at(1u), 7u);
  CHECK(array.invariants_hold());
  platform.fail_allocation_at(1u);
  CHECK_STATUS(array.resize(64u, 3u), Status::no_memory);
  CHECK_EQ(array.size(), 2u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_remove_at, closes_the_gap_in_order_and_ignores_an_index_past_the_end)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 0u; value < 5u; value++) CHECK_STATUS(array.append(value), Status::ok);
  array.remove_at(1u);
  CHECK(array.invariants_hold());
  CHECK_EQ(array.size(), 4u);
  CHECK_EQ(array.at(0u), 0u);
  CHECK_EQ(array.at(1u), 2u);
  CHECK_EQ(array.at(3u), 4u);
  array.remove_at(3u);
  CHECK_EQ(array.size(), 3u);
  CHECK_EQ(array.at(2u), 3u);
  array.remove_at(3u);
  array.remove_at(0xFFFFFFFFu);
  CHECK_EQ(array.size(), 3u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_remove_swap, moves_the_last_value_into_the_gap)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 0u; value < 4u; value++) CHECK_STATUS(array.append(value), Status::ok);
  array.remove_swap(0u);
  CHECK_EQ(array.size(), 3u);
  CHECK_EQ(array.at(0u), 3u);
  CHECK_EQ(array.at(1u), 1u);
  CHECK_EQ(array.at(2u), 2u);
  array.remove_swap(2u);
  CHECK_EQ(array.size(), 2u);
  CHECK_EQ(array.at(1u), 1u);
  array.remove_swap(2u);
  CHECK_EQ(array.size(), 2u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_truncate, cuts_only_downward_and_keeps_the_block)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 0u; value < 6u; value++) CHECK_STATUS(array.append(value), Status::ok);
  array.truncate(10u);
  CHECK_EQ(array.size(), 6u);
  array.truncate(2u);
  CHECK(holds_in_order(array, 0u, 2u));
  CHECK_EQ(array.capacity(), GrowableArray<std::uint32_t>::first_capacity);
  CHECK_EQ(platform.live_allocations(), 1u);
  CHECK(array.invariants_hold()); }

TEST(growable_array_clear, gives_the_block_back_and_the_array_grows_again)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  for (std::uint32_t value = 0u; value < 40u; value++) CHECK_STATUS(array.append(value), Status::ok);
  array.clear();
  CHECK(array.invariants_hold());
  CHECK_EQ(array.size(), 0u);
  CHECK_EQ(array.capacity(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(array.append(3u), Status::ok);
  CHECK(holds_in_order(array, 3u, 1u)); }

TEST(growable_array_iteration, walks_the_values_in_index_order)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  CHECK(array.begin() == array.end());
  for (std::uint32_t value = 10u; value < 15u; value++) CHECK_STATUS(array.append(value), Status::ok);
  std::uint32_t expected = 10u;
  for (std::uint32_t &value : array) {
      CHECK_EQ(value, expected++);
      value += 100u;
  }
  const GrowableArray<std::uint32_t> &view = array;
  CHECK_EQ(view.end() - view.begin(), 5);
  CHECK_EQ(*view.begin(), 110u);
  CHECK_EQ(view.at(4u), 114u); }

TEST(growable_array_destructor, gives_the_block_back)
{ MockPlatform platform;
  { GrowableArray<std::uint32_t> array(platform, Pool::paged);
    CHECK_STATUS(array.append(1u), Status::ok);
    CHECK_EQ(platform.live_allocations(), 1u); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(growable_array_invariants, fail_for_a_size_past_the_block_or_a_block_off_a_power_of_two)
{ MockPlatform platform;
  GrowableArray<std::uint32_t> array(platform, Pool::paged);
  GrowableArrayProbe::corrupt_size(array, 1u);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_size(array, 0u);
  GrowableArrayProbe::corrupt_capacity(array, 8u);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_capacity(array, 0u);
  CHECK_STATUS(array.append(1u), Status::ok);
  GrowableArrayProbe::corrupt_size(array, 9u);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_size(array, 1u);
  GrowableArrayProbe::corrupt_capacity(array, 12u);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_capacity(array, 4u);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_capacity(array, 0xFFFFFFFFu);
  CHECK(!array.invariants_hold());
  GrowableArrayProbe::corrupt_capacity(array, 8u);
  CHECK(array.invariants_hold()); }
