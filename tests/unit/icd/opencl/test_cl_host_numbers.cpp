#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_host_numbers.hpp"

#include <cstdint>

namespace av {
struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_size(GrowableArray<T> &array, std::uint32_t size) { array.size_ = size; }

    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};
}

namespace av::clicd {
struct ClHostNumbersProbe {
    static void spilled_to_a_few(ClHostNumbers &numbers) { GrowableArrayProbe::corrupt_size(numbers.spilled_, 3u); }
    static void spilled_storage_broken(ClHostNumbers &numbers)
    { GrowableArrayProbe::corrupt_capacity(numbers.spilled_, 12u); }
};
}

using av::clicd::ClHostNumbers;
using av::clicd::ClHostNumbersProbe;

TEST(cl_host_numbers, a_few_numbers_live_inline_and_a_fresh_set_holds_nothing_spilled)
{ av::MockPlatform platform;
  ClHostNumbers    numbers(platform);
  std::uint64_t   *few = numbers.room(ClHostNumbers::kept_inline);
  CHECK(few != nullptr);
  few[ClHostNumbers::kept_inline - 1u] = 7u;
  CHECK(numbers.room(1u) == few);
  CHECK_EQ(few[ClHostNumbers::kept_inline - 1u], 7u);
  CHECK(numbers.invariants_hold()); }

TEST(cl_host_numbers, more_numbers_than_the_inline_room_spill_to_memory_that_holds_them_all)
{ av::MockPlatform platform;
  ClHostNumbers    numbers(platform);
  const std::uint32_t many = ClHostNumbers::kept_inline + 10u;
  std::uint64_t      *room = numbers.room(many);
  CHECK(room != nullptr);
  CHECK(room != numbers.room(1u));
  for (std::uint32_t index = 0u; index < many; ++index) room[index] = index + 1u;
  CHECK_EQ(room[many - 1u], std::uint64_t{many});
  CHECK(numbers.invariants_hold());
  CHECK(numbers.room(many) == room); }

TEST(cl_host_numbers, memory_that_cannot_be_had_leaves_no_room)
{ av::MockPlatform platform;
  ClHostNumbers    numbers(platform);
  platform.fail_allocation_at(1u);
  CHECK(numbers.room(ClHostNumbers::kept_inline + 1u) == nullptr);
  CHECK(numbers.invariants_hold()); }

TEST(cl_host_numbers, the_invariant_catches_a_spill_that_is_not_one)
{ av::MockPlatform platform;
  { ClHostNumbers numbers(platform);
    CHECK(numbers.room(ClHostNumbers::kept_inline + 1u) != nullptr);
    ClHostNumbersProbe::spilled_storage_broken(numbers);
    CHECK(!numbers.invariants_hold()); }
  { ClHostNumbers numbers(platform);
    CHECK(numbers.room(ClHostNumbers::kept_inline + 1u) != nullptr);
    ClHostNumbersProbe::spilled_to_a_few(numbers);
    CHECK(!numbers.invariants_hold()); } }
