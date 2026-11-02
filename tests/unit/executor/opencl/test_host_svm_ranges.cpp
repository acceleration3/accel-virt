#include "executor/opencl/host_svm_ranges.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::HostSvmRanges;
using av::HostSvmRangesProbe;
using av::MockPlatform;
using av::Status;

namespace av {
struct HostSvmRangesProbe {
    static void a_range_of_nothing(HostSvmRanges &ranges) { ranges.ranges_.at(0u).size_ = 0u; }
    static void a_range_at_no_address(HostSvmRanges &ranges) { ranges.ranges_.at(0u).base_ = 0u; }
    static void a_range_that_wraps(HostSvmRanges &ranges)
    { ranges.ranges_.at(0u).size_ = ~std::uint64_t{0} - ranges.ranges_.at(0u).base_ + 1u; ranges.widest_ = ~std::uint64_t{0}; }
    static void two_rows_at_one_base(HostSvmRanges &ranges)
    { ranges.ranges_.at(1u).base_ = ranges.ranges_.at(0u).base_; }
    static void rows_out_of_order(HostSvmRanges &ranges)
    {
        const std::uint64_t first = ranges.ranges_.at(0u).base_;
        ranges.ranges_.at(0u).base_ = ranges.ranges_.at(1u).base_;
        ranges.ranges_.at(1u).base_ = first;
    }
    static void a_range_wider_than_the_widest(HostSvmRanges &ranges) { ranges.widest_ = 1u; }
    };

}

namespace {
constexpr uint64_t base = 0x0000'7F00'0000'1000ull;
constexpr uint64_t bytes = 0x1000ull;

void *at(uint64_t address) { return reinterpret_cast<void *>(static_cast<uintptr_t>(address)); }

}

TEST(host_svm_ranges, an_empty_table_resolves_nothing)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(0u, 0u) == nullptr);
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_address_it_allocated_resolves_to_itself)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.invariants_hold());
  CHECK(ranges.resolve(base, bytes) == at(base));
  CHECK_EQ(ranges.count(), 1u); }

TEST(host_svm_ranges, an_address_inside_one_resolves_because_a_kernel_argument_may_point_there)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.resolve(base + 1u, 0u) == at(base + 1u));
  CHECK(ranges.resolve(base + bytes - 1u, 1u) == at(base + bytes - 1u));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_extent_that_runs_past_the_end_is_refused)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.resolve(base, bytes) == at(base));
  CHECK(ranges.resolve(base, bytes + 1u) == nullptr);
  CHECK(ranges.resolve(base + 1u, bytes) == nullptr);
  CHECK(ranges.resolve(base + bytes, 0u) == nullptr);
  CHECK(ranges.resolve(base + bytes - 1u, 2u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_address_before_the_range_is_refused)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.resolve(base - 1u, 1u) == nullptr);
  CHECK(ranges.resolve(0u, 1u) == nullptr); }

TEST(host_svm_ranges, an_extent_that_would_wrap_is_refused_rather_than_wrapped)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.resolve(base, ~uint64_t{0}) == nullptr);
  CHECK(ranges.resolve(base + 1u, ~uint64_t{0} - 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_zero_length_allocation_holds_nothing_and_is_not_remembered)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), 0u, 0u), Status::invalid_parameter);
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 0u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_null_allocation_is_not_remembered)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(nullptr, bytes, 0u), Status::invalid_parameter);
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_allocation_that_would_wrap_is_not_remembered)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(~uint64_t{0} - 15u), 64u, 0u), Status::invalid_parameter);
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_freed_allocation_resolves_no_longer)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.forget(at(base)));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(base + 8u, 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, freeing_what_was_never_allocated_changes_nothing)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(!ranges.forget(at(base + 8u)));
  CHECK(!ranges.forget(at(base + bytes)));
  CHECK(!ranges.forget(nullptr));
  CHECK_EQ(ranges.count(), 1u);
  CHECK(ranges.resolve(base, 1u) == at(base));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, the_same_allocation_twice_is_held_once)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK_EQ(ranges.count(), 1u);
  CHECK(ranges.forget(at(base)));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_allocation_reused_at_the_same_address_takes_its_new_size)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK(ranges.forget(at(base)));
  CHECK_STATUS(ranges.remember(at(base), 16u, 0u), Status::ok);
  CHECK(ranges.resolve(base, 16u) == at(base));
  CHECK(ranges.resolve(base, 17u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, ranges_beside_each_other_do_not_answer_for_one_another)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK_STATUS(ranges.remember(at(base + bytes), bytes, 0u), Status::ok);
  CHECK_EQ(ranges.count(), 2u);
  CHECK(ranges.resolve(base + bytes - 8u, 16u) == nullptr);
  CHECK(ranges.resolve(base + bytes, bytes) == at(base + bytes));
  CHECK(ranges.forget(at(base)));
  CHECK(ranges.resolve(base + bytes, 1u) == at(base + bytes));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, holds_twenty_thousand_ranges_far_past_the_old_table)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  constexpr uint32_t held = 20000u;
  for (uint32_t index = 0u; index < held; ++index) {
      const uint64_t slot = (index * 7919u) % held;
      CHECK_STATUS(ranges.remember(at(base + 2u * slot * bytes), bytes, 0u), Status::ok);
  }
  CHECK_EQ(ranges.count(), held);
  CHECK(ranges.invariants_hold());
  for (uint32_t slot = 0u; slot < held; ++slot) {
      CHECK(ranges.resolve(base + 2u * slot * bytes + 5u, 8u) == at(base + 2u * slot * bytes + 5u));
      CHECK(ranges.resolve(base + (2u * slot + 1u) * bytes, 1u) == nullptr);
  }
  for (uint32_t slot = 0u; slot < held; slot += 2u) CHECK(ranges.forget(at(base + 2u * slot * bytes)));
  CHECK_EQ(ranges.count(), held / 2u);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(base + 2u * bytes, 1u) == at(base + 2u * bytes));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_table_that_cannot_grow_says_so_and_changes_nothing)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.make_room(), Status::ok);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  for (uint32_t index = 1u; index < 8u; ++index) CHECK_STATUS(ranges.remember(at(base + index * bytes), bytes, 0u), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(ranges.make_room(), Status::no_memory);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(ranges.remember(at(base - bytes), bytes, 0u), Status::no_memory);
  CHECK_EQ(ranges.count(), 8u);
  CHECK(ranges.resolve(base - bytes, 1u) == nullptr);
  CHECK(ranges.resolve(base + 7u * bytes, 1u) == at(base + 7u * bytes));
  CHECK(ranges.invariants_hold());
  platform.fail_allocation_at(1u);
  CHECK_STATUS(ranges.remember(at(base), 2u * bytes, 0u), Status::ok);
  platform.fail_allocation_at(0u);
  CHECK_STATUS(ranges.make_room(), Status::ok);
  CHECK_STATUS(ranges.remember(at(base - bytes), bytes, 0u), Status::ok);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, overlapping_ranges_each_answer_for_what_they_hold)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), 16u * bytes, 1u), Status::ok);
  CHECK_STATUS(ranges.remember(at(base + bytes), 16u, 1u), Status::ok);
  CHECK_STATUS(ranges.remember(at(base + 2u * bytes), 16u, 1u), Status::ok);
  CHECK(ranges.resolve(base + 10u * bytes, bytes) == at(base + 10u * bytes));
  CHECK(ranges.resolve(base + 16u * bytes - 1u, 1u) == at(base + 16u * bytes - 1u));
  CHECK(ranges.resolve(base + 16u * bytes, 1u) == nullptr);
  CHECK(ranges.forget(at(base)));
  CHECK(ranges.resolve(base + 10u * bytes, 1u) == nullptr);
  CHECK(ranges.resolve(base + bytes + 4u, 4u) == at(base + bytes + 4u));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, forgetting_everything_leaves_nothing_resolving)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  CHECK_STATUS(ranges.remember(at(base), bytes, 0u), Status::ok);
  CHECK_STATUS(ranges.remember(at(base + bytes), bytes, 0u), Status::ok);
  ranges.forget_all();
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(base + bytes, 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, the_invariant_catches_the_states_it_is_there_to_catch)
{ std::uint8_t first[64] = {};
  std::uint8_t second[64] = {};
  const auto broken = [&first, &second](void (*corrupt)(HostSvmRanges &)) {
      MockPlatform  platform;
      HostSvmRanges ranges(platform);
      CHECK_STATUS(ranges.remember(first, sizeof first, 0u), Status::ok);
      CHECK_STATUS(ranges.remember(second, sizeof second, 0u), Status::ok);
      CHECK(ranges.invariants_hold());
      corrupt(ranges);
      return ranges.invariants_hold();
  };
  CHECK(!broken(HostSvmRangesProbe::a_range_of_nothing));
  CHECK(!broken(HostSvmRangesProbe::a_range_at_no_address));
  CHECK(!broken(HostSvmRangesProbe::a_range_that_wraps));
  CHECK(!broken(HostSvmRangesProbe::two_rows_at_one_base));
  CHECK(!broken(HostSvmRangesProbe::rows_out_of_order));
  CHECK(!broken(HostSvmRangesProbe::a_range_wider_than_the_widest)); }

TEST(host_svm_ranges, a_released_buffer_forgets_its_mappings_and_no_one_elses)
{ MockPlatform platform;
  HostSvmRanges ranges(platform);
  constexpr std::uint64_t buffer = 0xb0ffu, other = 0xb1ffu;
  CHECK_STATUS(ranges.remember(at(0x10000u), 64u, buffer), Status::ok);
  CHECK_STATUS(ranges.remember(at(0x20000u), 64u, other), Status::ok);
  CHECK_STATUS(ranges.remember(at(0x30000u), 64u, buffer), Status::ok);
  CHECK_STATUS(ranges.remember(at(0x40000u), 64u, 0u), Status::ok);
  CHECK_EQ(ranges.forget_owned(buffer), 2u);
  CHECK_EQ(ranges.count(), 2u);
  CHECK(ranges.resolve(0x10000u, 1u) == nullptr);
  CHECK(ranges.resolve(0x30000u, 1u) == nullptr);
  CHECK(ranges.resolve(0x20000u, 1u) != nullptr);
  CHECK(ranges.resolve(0x40000u, 1u) != nullptr);
  CHECK_EQ(ranges.forget_owned(0u), 0u);
  CHECK_EQ(ranges.forget_owned(buffer), 0u);
  CHECK_EQ(ranges.count(), 2u);
  CHECK(ranges.invariants_hold()); }
