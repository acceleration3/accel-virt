#include "executor/opencl/kernel_pacer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::KernelPacer;

namespace av {

struct KernelPacerProbe {
    static void shrink_to_nothing(KernelPacer &pacer) { pacer.group_ = 0u; }
    static void overgather(KernelPacer &pacer) { pacer.gathered_ = KernelPacer::most + 1u; }
};

}

namespace {

constexpr std::uint64_t millisecond = 1000000u;

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

}

TEST(kernel_pacer, flushes_every_kernel_until_a_group_is_timed)
{ KernelPacer pacer;
  CHECK_EQ(pacer.group(), 1u);
  for (int kernel = 0; kernel < 3; kernel++) {
      CHECK(pacer.kernel());
      pacer.flushed();
  }
  CHECK(pacer.invariants_hold()); }

TEST(kernel_pacer, a_slow_kernel_makes_small_groups)
{ KernelPacer pacer;
  time_a_group(pacer, 0u, 17u * millisecond);
  CHECK_EQ(pacer.group(), static_cast<std::uint32_t>(KernelPacer::budget_ns / (17u * millisecond)));
  CHECK_EQ(pacer.group(), 11u);
  time_a_group(pacer, 0u, 2200u * millisecond);
  CHECK_EQ(pacer.group(), 1u); }

TEST(kernel_pacer, fast_kernels_fill_the_largest_group)
{ KernelPacer pacer;
  time_a_group(pacer, 1000u, 1000u + 50000u);
  CHECK_EQ(pacer.group(), KernelPacer::most);
  time_a_group(pacer, 5u, 5u);
  CHECK_EQ(pacer.group(), KernelPacer::most);
  time_a_group(pacer, 10u, 3u);
  CHECK_EQ(pacer.group(), KernelPacer::most); }

TEST(kernel_pacer, a_guest_flush_starts_the_group_again)
{ KernelPacer pacer;
  time_a_group(pacer, 0u, 20u * millisecond);
  CHECK_EQ(pacer.group(), 10u);
  for (int kernel = 0; kernel < 9; kernel++) CHECK(!pacer.kernel());
  pacer.flushed();
  for (int kernel = 0; kernel < 9; kernel++) CHECK(!pacer.kernel());
  CHECK(pacer.kernel()); }

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
