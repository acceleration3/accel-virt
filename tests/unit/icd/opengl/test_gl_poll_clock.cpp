#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/opengl/gl_poll_clock.hpp"

#include <cstdint>

using av::MockKmt;
using av::glicd::GlPollClock;

namespace {

constexpr std::uint64_t step_ticks = 1000u;
constexpr std::uint64_t step_nanoseconds = step_ticks * 1000000000u / MockKmt::ticks_per_second;

}

TEST(gl_poll_clock, a_clock_bound_to_nothing_reads_zero_and_pauses_not_at_all)
{ GlPollClock clock;
  CHECK_EQ(clock.now_nanoseconds(), 0u);
  clock.pause(1000000u);
  CHECK_EQ(clock.now_nanoseconds(), 0u); }

TEST(gl_poll_clock, the_time_is_the_kernels_counter_in_nanoseconds)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, step_ticks);
  GlPollClock clock;
  clock.bind(kmt);
  CHECK_EQ(clock.now_nanoseconds(), step_nanoseconds);
  CHECK_EQ(clock.now_nanoseconds(), 2u * step_nanoseconds);
  kmt.set_frequency(0u);
  CHECK_EQ(clock.now_nanoseconds(), 0u); }

TEST(gl_poll_clock, a_pause_sleeps_briefly_until_the_time_asked_for_has_passed)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, step_ticks);
  GlPollClock clock;
  clock.bind(kmt);
  const std::uint64_t before = clock.now_nanoseconds();
  clock.pause(2u * step_nanoseconds + step_nanoseconds / 2u);
  CHECK_EQ(kmt.sleeps(), 3u);
  CHECK(clock.now_nanoseconds() >= before + 2u * step_nanoseconds + step_nanoseconds / 2u); }

TEST(gl_poll_clock, a_pause_of_nothing_still_sleeps_once)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, step_ticks);
  GlPollClock clock;
  clock.bind(kmt);
  clock.pause(0u);
  CHECK_EQ(kmt.sleeps(), 1u); }
