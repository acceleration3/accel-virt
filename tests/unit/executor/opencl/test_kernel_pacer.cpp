/* How many kernels go to the host GPU in one submission: one at a time until a
 * group has been timed, then as many as fit the budget at the time each took,
 * between one and KernelPacer::most. */
#include "executor/opencl/kernel_pacer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::KernelPacer;

namespace av {

struct KernelPacerProbe {
    static void shrink_to_nothing(KernelPacer &pacer) { pacer.group_ = 0u; }
    static void overgather(KernelPacer &pacer) { pacer.gathered_ = KernelPacer::most + 1u; }
};

}  // namespace av

namespace {

constexpr std::uint64_t millisecond = 1000000u;

/* A group of the current size, flushed at `from` and timed at `to`. */
void time_a_group(KernelPacer &pacer, std::uint64_t from, std::uint64_t to)
{
    for (std::uint32_t kernel = 1u; kernel < pacer.group(); kernel++) CHECK(!pacer.kernel());
    CHECK(pacer.kernel());
    CHECK(!pacer.timing());
    pacer.time_from(from);
    pacer.flushed();
    CHECK(pacer.timing());
    pacer.timed(to);
    CHECK(!pacer.timing());
    CHECK(pacer.invariants_hold());
}

}  // namespace

/* Nothing timed yet, nothing is known of the GPU: every kernel is its own job. */
TEST(kernel_pacer, flushes_every_kernel_until_a_group_is_timed)
{ KernelPacer pacer;
  CHECK_EQ(pacer.group(), 1u);
  for (int kernel = 0; kernel < 3; kernel++) {
      CHECK(pacer.kernel());
      pacer.flushed();
  }
  CHECK(pacer.invariants_hold()); }

/* nbody on a Radeon 610M: 17 ms a step fits eleven in the budget. */
TEST(kernel_pacer, a_slow_kernel_makes_small_groups)
{ KernelPacer pacer;
  time_a_group(pacer, 0u, 17u * millisecond);
  CHECK_EQ(pacer.group(), static_cast<std::uint32_t>(KernelPacer::budget_ns / (17u * millisecond)));
  CHECK_EQ(pacer.group(), 11u);
  /* A group of eleven that took 2.2 s - the GPU busy elsewhere - shrinks it
   * to one. */
  time_a_group(pacer, 0u, 2200u * millisecond);
  CHECK_EQ(pacer.group(), 1u); }

/* Small kernels on a fast GPU fill the largest group, and no more. */
TEST(kernel_pacer, fast_kernels_fill_the_largest_group)
{ KernelPacer pacer;
  time_a_group(pacer, 1000u, 1000u + 50000u);
  CHECK_EQ(pacer.group(), KernelPacer::most);
  time_a_group(pacer, 5u, 5u);   /* too quick to tell */
  CHECK_EQ(pacer.group(), KernelPacer::most);
  time_a_group(pacer, 10u, 3u);  /* a clock that went back is no time at all */
  CHECK_EQ(pacer.group(), KernelPacer::most); }

/* A flush the guest asked for ends the group it gathered: the count starts
 * again. A group too long for the budget is one kernel, never none. */
TEST(kernel_pacer, a_guest_flush_starts_the_group_again)
{ KernelPacer pacer;
  time_a_group(pacer, 0u, 20u * millisecond);
  CHECK_EQ(pacer.group(), 10u);
  for (int kernel = 0; kernel < 9; kernel++) CHECK(!pacer.kernel());
  pacer.flushed();
  for (int kernel = 0; kernel < 9; kernel++) CHECK(!pacer.kernel());
  CHECK(pacer.kernel()); }

/* A marker seen complete with no group being timed changes nothing. */
TEST(kernel_pacer, a_timing_with_nothing_timed_changes_nothing)
{ KernelPacer pacer;
  pacer.timed(5u * millisecond);
  CHECK_EQ(pacer.group(), 1u);
  CHECK(!pacer.timing()); }

TEST(kernel_pacer_invariants, an_empty_group_or_too_much_gathered_is_detected)
{ KernelPacer empty;
  av::KernelPacerProbe::shrink_to_nothing(empty);
  CHECK(!empty.invariants_hold());
  KernelPacer full;
  av::KernelPacerProbe::overgather(full);
  CHECK(!full.invariants_hold()); }
