/* A wait timed on the guest's performance counter (CounterWait): the first
 * slice at once, then slices while the deadline has not passed on the counter
 * the Kmt reads, and none after it. MockKmt's clock moves half a second each
 * time it is read, so the deadline passes on its own. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/counter_wait.hpp"
#include "icd/common/sliced_wait.hpp"

#include <cstdint>

using av::icd::CounterWait;
using av::icd::SlicedWait;

namespace {

constexpr std::uint64_t half_second = av::MockKmt::ticks_per_second / 2u;

}  // namespace

TEST(counter_wait, slices_until_the_counter_passes_the_deadline)
{ av::MockKmt kmt;
  kmt.reads_clock(nullptr, half_second);
  CounterWait   wait(kmt, 3u * SlicedWait::slice_ns);
  std::uint64_t slice = 0u;
  std::uint32_t asked = 0u;
  while (wait.next(slice) && asked < 100u) {
      CHECK(slice <= SlicedWait::slice_ns);
      asked++;
  }
  /* A second and a half of half-second reads: a few slices, never forever. */
  CHECK(asked >= 2u && asked <= 4u); }

TEST(counter_wait, a_zero_timeout_still_asks_once)
{ av::MockKmt kmt;
  kmt.reads_clock(nullptr, half_second);
  CounterWait   wait(kmt, 0u);
  std::uint64_t slice = 9u;
  CHECK(wait.next(slice));
  CHECK_EQ(slice, 0u);
  CHECK(!wait.next(slice)); }

/* Forever never passes, however far the clock moves. */
TEST(counter_wait, forever_keeps_asking)
{ av::MockKmt kmt;
  kmt.reads_clock(nullptr, 100u * av::MockKmt::ticks_per_second);
  CounterWait   wait(kmt, SlicedWait::forever);
  std::uint64_t slice = 0u;
  for (std::uint32_t round = 0u; round < 16u; round++) CHECK(wait.next(slice));
  CHECK_EQ(slice, SlicedWait::slice_ns); }
