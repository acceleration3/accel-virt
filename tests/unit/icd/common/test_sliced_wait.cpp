#include "harness/framework/test_registry.hpp"
#include "icd/common/sliced_wait.hpp"

#include <cstdint>

using av::icd::SlicedWait;

namespace av::icd {

struct SlicedWaitProbe {
    static std::uint64_t &slice(SlicedWait &wait) { return wait.slice_; }
    static bool          &asked(SlicedWait &wait) { return wait.asked_; }
};

}

using av::icd::SlicedWaitProbe;

namespace {

constexpr std::uint64_t ms = 1000000u;
constexpr std::uint64_t ticks_per_second = 10000000u;
constexpr std::uint64_t ticks_per_ms = ticks_per_second / 1000u;
constexpr std::uint64_t max = ~std::uint64_t{0};

}

TEST(sliced_wait, a_timeout_of_1200_ms_is_three_slices_as_the_clock_moves_500_ms_each)
{ const std::uint64_t start = 7u * ticks_per_second;
  SlicedWait wait(1200u * ms, start, ticks_per_second);
  CHECK(wait.invariants_hold());
  const std::uint64_t expected[] = {500u * ms, 500u * ms, 200u * ms};
  std::uint64_t now = start;
  std::uint64_t slice = 0u;
  for (const std::uint64_t sent : expected) {
      CHECK(wait.next(now, slice));
      CHECK(wait.invariants_hold());
      CHECK_EQ(slice, sent);
      now += 500u * ticks_per_ms;
  }
  CHECK(!wait.next(now, slice));
  CHECK(wait.invariants_hold()); }

TEST(sliced_wait, the_first_slice_is_sent_whatever_the_clock_says)
{ SlicedWait wait(1200u * ms, 0u, ticks_per_second);
  std::uint64_t slice = 0u;
  CHECK(wait.next(max, slice));
  CHECK_EQ(slice, 500u * ms);
  CHECK(!wait.next(max, slice)); }

TEST(sliced_wait, a_zero_timeout_is_one_zero_length_slice)
{ SlicedWait wait(0u, 100u, ticks_per_second);
  std::uint64_t slice = 99u;
  CHECK(wait.next(100u, slice));
  CHECK(wait.invariants_hold());
  CHECK_EQ(slice, 0u);
  CHECK(!wait.next(100u, slice)); }

TEST(sliced_wait, a_timeout_shorter_than_a_slice_is_sent_whole)
{ SlicedWait wait(1u, 0u, ticks_per_second);
  std::uint64_t slice = 0u;
  CHECK(wait.next(0u, slice));
  CHECK_EQ(slice, 1u);
  CHECK(wait.next(0u, slice));
  CHECK_EQ(slice, 1u);
  CHECK(!wait.next(1u, slice)); }

TEST(sliced_wait, the_deadline_passes_at_exactly_the_timeout_and_not_a_tick_before)
{ SlicedWait wait(1000u * ms, 0u, ticks_per_second);
  std::uint64_t slice = 0u;
  CHECK(wait.next(0u, slice));
  CHECK(wait.next(ticks_per_second - 1u, slice));
  CHECK_EQ(slice, 100u);
  CHECK(!wait.next(ticks_per_second, slice)); }

TEST(sliced_wait, an_infinite_timeout_never_passes_even_at_the_counters_end)
{ SlicedWait wait(SlicedWait::forever, max - 10u, 1u);
  std::uint64_t slice = 0u;
  const std::uint64_t moments[] = {max - 10u, max, 0u, max / 2u};
  for (const std::uint64_t now : moments) {
      CHECK(wait.next(now, slice));
      CHECK(wait.invariants_hold());
      CHECK_EQ(slice, SlicedWait::slice_ns);
  }
}

TEST(sliced_wait, a_counter_with_no_frequency_gives_a_finite_wait_one_slice)
{ SlicedWait finite(1200u * ms, 0u, 0u);
  std::uint64_t slice = 0u;
  CHECK(finite.next(0u, slice));
  CHECK_EQ(slice, 500u * ms);
  CHECK(!finite.next(0u, slice));
  SlicedWait infinite(SlicedWait::forever, 0u, 0u);
  CHECK(infinite.next(0u, slice));
  CHECK(infinite.next(0u, slice));
  CHECK_EQ(slice, SlicedWait::slice_ns); }

TEST(sliced_wait, a_counter_that_went_backwards_has_measured_nothing)
{ SlicedWait wait(1200u * ms, 50u * ticks_per_second, ticks_per_second);
  std::uint64_t slice = 0u;
  CHECK(wait.next(50u * ticks_per_second, slice));
  CHECK(wait.next(0u, slice));
  CHECK_EQ(slice, 500u * ms); }

TEST(sliced_wait, elapsed_time_saturates_at_every_overflow_rather_than_wrapping)
{ struct Case {
      const char   *why;
      std::uint64_t frequency;
      std::uint64_t now;
  };
  const Case cases[] = {
      {"whole seconds times 10^9", 1u, max},
      {"a part of a second times 10^9", max, max - 1u},
      {"their sum", 999999999u, 18446744055262807543ull},
  };
  for (const Case &check : cases) {
      SlicedWait wait(max - 1u, 0u, check.frequency);
      std::uint64_t slice = 0u;
      CHECK(wait.next(0u, slice));
      if (wait.next(check.now, slice))
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s: not passed", check.why);
  }
  SlicedWait short_of(max, 0u, 999999999u);
  std::uint64_t slice = 0u;
  CHECK(short_of.next(0u, slice));
  CHECK(short_of.next(18446744055262807542ull, slice)); }

TEST(sliced_wait, invariants_refuse_a_slice_longer_than_allowed_or_sent_before_asking)
{ SlicedWait wait(200u * ms, 0u, ticks_per_second);
  std::uint64_t slice = 0u;
  CHECK(wait.next(0u, slice));
  CHECK(wait.invariants_hold());
  SlicedWaitProbe::slice(wait) = 201u * ms;
  CHECK(!wait.invariants_hold());
  SlicedWait long_wait(SlicedWait::forever, 0u, ticks_per_second);
  CHECK(long_wait.next(0u, slice));
  SlicedWaitProbe::slice(long_wait) = SlicedWait::slice_ns + 1u;
  CHECK(!long_wait.invariants_hold());
  SlicedWait unasked(SlicedWait::forever, 0u, ticks_per_second);
  SlicedWaitProbe::slice(unasked) = 1u;
  CHECK(!unasked.invariants_hold());
  SlicedWaitProbe::asked(unasked) = true;
  CHECK(unasked.invariants_hold()); }
