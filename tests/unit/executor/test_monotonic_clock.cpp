#include "executor/monotonic_clock.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::MonotonicClock;

namespace {

std::uint64_t nanoseconds_of(const timespec &at)
{
    return static_cast<std::uint64_t>(at.tv_sec) * 1000000000u + static_cast<std::uint64_t>(at.tv_nsec);
}

}

TEST(monotonic_clock, never_goes_back)
{ MonotonicClock    clock;
  const std::uint64_t first = clock.now_nanoseconds();
  CHECK(first > 0u);
  CHECK(clock.now_nanoseconds() >= first); }

TEST(monotonic_clock, a_pause_sleeps_at_least_as_long_as_asked)
{ MonotonicClock      clock;
  const std::uint64_t started = clock.now_nanoseconds();
  clock.pause(3000000u);
  CHECK(clock.now_nanoseconds() - started >= 3000000u); }

TEST(monotonic_clock, a_pause_past_a_second_is_whole)
{ MonotonicClock      clock;
  const std::uint64_t started = clock.now_nanoseconds();
  clock.pause(1000000001u);
  CHECK(clock.now_nanoseconds() - started >= 1000000001u); }

TEST(monotonic_clock, a_zero_pause_only_yields)
{ MonotonicClock      clock;
  const std::uint64_t started = clock.now_nanoseconds();
  clock.pause(0u);
  CHECK(clock.now_nanoseconds() - started < 100000000u); }

TEST(monotonic_clock, ahead_is_that_far_from_now_on_the_same_clock)
{ MonotonicClock      clock;
  const std::uint64_t before = clock.now_nanoseconds();
  const timespec      at = clock.ahead(2500000000u);
  const std::uint64_t after = clock.now_nanoseconds();
  CHECK(at.tv_nsec >= 0 && at.tv_nsec < 1000000000);
  CHECK(nanoseconds_of(at) >= before + 2500000000u);
  CHECK(nanoseconds_of(at) <= after + 2500000000u); }
