/* The bounds check that stands between a guest's number and the host's own
 * memory.
 *
 * A guest holding shared virtual memory hands its addresses back - to a kernel
 * argument, to a map, to a copy - and each is a number it could have invented.
 * What matters here: only an address inside something this session allocated
 * resolves, the extent the command will touch is part of the question, the
 * arithmetic that asks it cannot wrap, and nothing survives the allocation
 * being freed or the session shutting down. */
#include "executor/opencl/host_svm_ranges.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::HostSvmRanges;
using av::HostSvmRangesProbe;

namespace av {

/* The states the table never builds. Two rows at one base would make a free
 * ambiguous; a row past the count is a range that resolve() skips but that
 * still holds an address; and a range whose sum wraps could be resolved
 * through either end. */
struct HostSvmRangesProbe {
    static void count_past_the_table(HostSvmRanges &ranges)
    { ranges.count_ = HostSvmRanges::max_ranges + 1u; }
    static void a_range_of_nothing(HostSvmRanges &ranges) { ranges.sizes_[0] = 0u; }
    static void a_range_at_no_address(HostSvmRanges &ranges) { ranges.bases_[0] = 0u; }
    static void a_range_that_wraps(HostSvmRanges &ranges)
    { ranges.sizes_[0] = ~std::uint64_t{0} - ranges.bases_[0] + 1u; }
    static void two_rows_at_one_base(HostSvmRanges &ranges)
    { ranges.bases_[1] = ranges.bases_[0]; }
    static void a_base_behind_the_count(HostSvmRanges &ranges)
    { ranges.bases_[ranges.count_] = 0x1000u; }
    static void a_length_behind_the_count(HostSvmRanges &ranges)
    { ranges.sizes_[ranges.count_] = 0x1000u; }
};

}  // namespace av

namespace {

/* Addresses, not memory: nothing here is dereferenced, and the class must not
 * either. Far from any real allocation so a bug that reads one shows as a
 * crash rather than as a pass. */
constexpr uint64_t base = 0x0000'7F00'0000'1000ull;
constexpr uint64_t bytes = 0x1000ull;

void *at(uint64_t address) { return reinterpret_cast<void *>(static_cast<uintptr_t>(address)); }

}  // namespace

TEST(host_svm_ranges, an_empty_table_resolves_nothing)
{ HostSvmRanges ranges;
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(0u, 0u) == nullptr);
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_address_it_allocated_resolves_to_itself)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.invariants_hold());
  CHECK(ranges.resolve(base, bytes) == at(base));
  CHECK_EQ(ranges.count(), 1u); }

TEST(host_svm_ranges, an_address_inside_one_resolves_because_a_kernel_argument_may_point_there)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.resolve(base + 1u, 0u) == at(base + 1u));
  CHECK(ranges.resolve(base + bytes - 1u, 1u) == at(base + bytes - 1u));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_extent_that_runs_past_the_end_is_refused)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  /* The last byte is in; one past it is not, however it is asked for. */
  CHECK(ranges.resolve(base, bytes) == at(base));
  CHECK(ranges.resolve(base, bytes + 1u) == nullptr);
  CHECK(ranges.resolve(base + 1u, bytes) == nullptr);
  CHECK(ranges.resolve(base + bytes, 0u) == nullptr);
  CHECK(ranges.resolve(base + bytes - 1u, 2u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_address_before_the_range_is_refused)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.resolve(base - 1u, 1u) == nullptr);
  CHECK(ranges.resolve(0u, 1u) == nullptr); }

TEST(host_svm_ranges, an_extent_that_would_wrap_is_refused_rather_than_wrapped)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  /* address + bytes overflows; computed in the obvious way it lands back
   * inside the range and answers a pointer. */
  CHECK(ranges.resolve(base, ~uint64_t{0}) == nullptr);
  CHECK(ranges.resolve(base + 1u, ~uint64_t{0} - 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_zero_length_allocation_holds_nothing_and_is_not_remembered)
{ HostSvmRanges ranges;
  CHECK(!ranges.remember(at(base), 0u, 0u));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 0u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_null_allocation_is_not_remembered)
{ HostSvmRanges ranges;
  CHECK(!ranges.remember(nullptr, bytes, 0u));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_allocation_that_would_wrap_is_not_remembered)
{ HostSvmRanges ranges;
  CHECK(!ranges.remember(at(~uint64_t{0} - 15u), 64u, 0u));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_freed_allocation_resolves_no_longer)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.forget(at(base)));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(base + 8u, 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, freeing_what_was_never_allocated_changes_nothing)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  /* The API frees by the pointer clSVMAlloc answered, so an address inside
   * the range is not that pointer and frees nothing. */
  CHECK(!ranges.forget(at(base + 8u)));
  CHECK(!ranges.forget(at(base + bytes)));
  CHECK(!ranges.forget(nullptr));
  CHECK_EQ(ranges.count(), 1u);
  CHECK(ranges.resolve(base, 1u) == at(base));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, the_same_allocation_twice_is_held_once)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK_EQ(ranges.count(), 1u);
  CHECK(ranges.forget(at(base)));
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, an_allocation_reused_at_the_same_address_takes_its_new_size)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.forget(at(base)));
  CHECK(ranges.remember(at(base), 16u, 0u));
  CHECK(ranges.resolve(base, 16u) == at(base));
  CHECK(ranges.resolve(base, 17u) == nullptr);
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, ranges_beside_each_other_do_not_answer_for_one_another)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.remember(at(base + bytes), bytes, 0u));
  CHECK_EQ(ranges.count(), 2u);
  /* A span across the seam belongs to neither, however adjacent they are. */
  CHECK(ranges.resolve(base + bytes - 8u, 16u) == nullptr);
  CHECK(ranges.resolve(base + bytes, bytes) == at(base + bytes));
  CHECK(ranges.forget(at(base)));
  CHECK(ranges.resolve(base + bytes, 1u) == at(base + bytes));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, a_table_that_is_full_refuses_and_stays_usable)
{ HostSvmRanges ranges;
  for (uint32_t index = 0u; index < HostSvmRanges::max_ranges; ++index) {
      CHECK(ranges.remember(at(base + index * bytes), bytes, 0u));
  }
  CHECK_EQ(ranges.count(), HostSvmRanges::max_ranges);
  CHECK(!ranges.remember(at(base + HostSvmRanges::max_ranges * bytes), bytes, 0u));
  CHECK(ranges.resolve(base, 1u) == at(base));
  CHECK(ranges.invariants_hold());
  CHECK(ranges.forget(at(base)));
  CHECK(ranges.remember(at(base + HostSvmRanges::max_ranges * bytes), bytes, 0u));
  CHECK(ranges.invariants_hold()); }

TEST(host_svm_ranges, forgetting_everything_leaves_nothing_resolving)
{ HostSvmRanges ranges;
  CHECK(ranges.remember(at(base), bytes, 0u));
  CHECK(ranges.remember(at(base + bytes), bytes, 0u));
  ranges.forget_all();
  CHECK_EQ(ranges.count(), 0u);
  CHECK(ranges.resolve(base, 1u) == nullptr);
  CHECK(ranges.resolve(base + bytes, 1u) == nullptr);
  CHECK(ranges.invariants_hold()); }

/* The invariant catches the states the table never builds. */
TEST(host_svm_ranges, the_invariant_catches_the_states_it_is_there_to_catch)
{ std::uint8_t first[64] = {};
  std::uint8_t second[64] = {};
  const auto broken = [&first, &second](void (*corrupt)(HostSvmRanges &)) {
      HostSvmRanges ranges;
      CHECK(ranges.remember(first, sizeof first, 0u));
      CHECK(ranges.remember(second, sizeof second, 0u));
      CHECK(ranges.invariants_hold());
      corrupt(ranges);
      return ranges.invariants_hold();
  };
  CHECK(!broken(HostSvmRangesProbe::count_past_the_table));
  CHECK(!broken(HostSvmRangesProbe::a_range_of_nothing));
  CHECK(!broken(HostSvmRangesProbe::a_range_at_no_address));
  CHECK(!broken(HostSvmRangesProbe::a_range_that_wraps));
  CHECK(!broken(HostSvmRangesProbe::two_rows_at_one_base));
  CHECK(!broken(HostSvmRangesProbe::a_base_behind_the_count));
  CHECK(!broken(HostSvmRangesProbe::a_length_behind_the_count)); }

/* A mapped buffer released before it was unmapped takes its ranges with it,
 * and only its own: nothing names those addresses again, and a session whose
 * buffers were all released mapped would otherwise fill the table. */
TEST(host_svm_ranges, a_released_buffer_forgets_its_mappings_and_no_one_elses)
{ HostSvmRanges ranges;
  constexpr std::uint64_t buffer = 0xb0ffu, other = 0xb1ffu;
  CHECK(ranges.remember(at(0x10000u), 64u, buffer));
  CHECK(ranges.remember(at(0x20000u), 64u, other));
  CHECK(ranges.remember(at(0x30000u), 64u, buffer));
  CHECK(ranges.remember(at(0x40000u), 64u, 0u));
  CHECK_EQ(ranges.forget_owned(buffer), 2u);
  CHECK_EQ(ranges.count(), 2u);
  CHECK(ranges.resolve(0x10000u, 1u) == nullptr);
  CHECK(ranges.resolve(0x30000u, 1u) == nullptr);
  CHECK(ranges.resolve(0x20000u, 1u) != nullptr);
  CHECK(ranges.resolve(0x40000u, 1u) != nullptr);
  /* No owner is no buffer: the unowned range stays. */
  CHECK_EQ(ranges.forget_owned(0u), 0u);
  CHECK_EQ(ranges.forget_owned(buffer), 0u);
  CHECK_EQ(ranges.count(), 2u);
  CHECK(ranges.invariants_hold()); }
