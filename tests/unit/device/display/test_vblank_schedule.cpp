/* A head's blanks: the first one period after the schedule starts, each after
 * that at exactly n periods - the rate's fraction of a nanosecond carried, so
 * sixty blanks at 60 Hz are one second to the nanosecond - every blank that
 * passed counted however late the count comes, and a new rate taking over
 * from the last blank without the sequence moving back or skipping. */
#include "device/display/vblank_schedule.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::VblankSchedule;

namespace {

constexpr std::uint64_t second = 1000000000u;
constexpr std::uint64_t start_at = 5000u;

}  // namespace

TEST(vblank_schedule, counts_nothing_before_it_starts)
{ VblankSchedule schedule;
  CHECK(!schedule.running());
  CHECK_EQ(schedule.refresh_millihz(), VblankSchedule::default_millihz);
  CHECK_EQ(schedule.advance(10u * second), 0u);
  CHECK_EQ(schedule.sequence(), 0u);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, the_first_blank_is_a_period_after_the_start_and_each_is_counted_once)
{ VblankSchedule schedule;
  schedule.start(start_at);
  CHECK(schedule.running());
  CHECK(schedule.invariants_hold());
  CHECK_EQ(schedule.next(), start_at + 16666666u);
  CHECK_EQ(schedule.advance(start_at + 16666665u), 0u);
  CHECK_EQ(schedule.sequence(), 0u);
  CHECK_EQ(schedule.advance(start_at + 16666666u), 1u);
  CHECK_EQ(schedule.sequence(), 1u);
  CHECK_EQ(schedule.last(), start_at + 16666666u);
  CHECK_EQ(schedule.advance(start_at + 16666666u), 0u);   /* the same moment again counts nothing */
  CHECK_EQ(schedule.next(), start_at + 33333333u);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, sixty_blanks_at_sixty_hertz_are_one_second_to_the_nanosecond)
{ VblankSchedule schedule;
  schedule.start(start_at);
  std::uint64_t previous = start_at;
  std::uint32_t short_gaps = 0u;
  for (std::uint32_t blank = 1u; blank <= 60u; blank++) {
      const std::uint64_t due = schedule.next();
      CHECK_EQ(schedule.advance(due), 1u);
      const std::uint64_t gap = due - previous;
      CHECK(gap == 16666666u || gap == 16666667u);
      short_gaps += gap == 16666666u ? 1u : 0u;
      previous = due;
  }
  CHECK_EQ(schedule.sequence(), 60u);
  CHECK_EQ(schedule.last(), start_at + second);
  CHECK_EQ(short_gaps, 20u);   /* two thirds of a nanosecond short each time, made up every third */
   CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, one_hundred_twenty_hertz_carries_fractional_nanoseconds_without_drift)
{ VblankSchedule schedule;
  constexpr std::uint64_t epoch = 7u * 24u * 60u * 60u * second;
  schedule.retime(120000u, epoch);
  schedule.start(epoch);
  std::uint64_t previous = epoch;
  for (std::uint64_t blank = 1u; blank <= 120u * 60u * 60u; blank++) {
      const std::uint64_t due = schedule.next();
      CHECK_EQ(due, epoch + blank * second / 120u);
      CHECK_EQ(schedule.advance(due - 1u), 0u);
      CHECK_EQ(schedule.advance(due), 1u);
      CHECK(due - previous == 8333333u || due - previous == 8333334u);
      previous = due;
  }
  CHECK_EQ(schedule.last(), epoch + 60u * 60u * second);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, a_late_count_at_one_hundred_twenty_hertz_keeps_the_absolute_phase)
{ VblankSchedule schedule;
  schedule.retime(120000u, start_at);
  schedule.start(start_at);
  CHECK_EQ(schedule.advance(start_at + second + 1234567u), 120u);
  CHECK_EQ(schedule.last(), start_at + second);
  CHECK_EQ(schedule.next(), start_at + second + 8333333u);
  CHECK_EQ(schedule.advance(start_at + second + 8333332u), 0u);
  CHECK_EQ(schedule.advance(start_at + second + 8333333u), 1u);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, a_late_count_takes_every_blank_that_passed)
{ VblankSchedule schedule;
  schedule.start(start_at);
  CHECK_EQ(schedule.advance(start_at + 5u * 16666667u + 100u), 5u);
  CHECK_EQ(schedule.sequence(), 5u);
  CHECK_EQ(schedule.last(), start_at + 83333333u);
  CHECK_EQ(schedule.next(), start_at + 100000000u);   /* the phase is the one it started with */
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, a_new_rate_runs_from_the_last_blank_and_the_sequence_carries_on)
{ VblankSchedule schedule;
  schedule.start(start_at);
  CHECK_EQ(schedule.advance(start_at + 3u * 16666667u), 3u);
  const std::uint64_t last = schedule.last();
  schedule.retime(144000u, last + 1000u);
  CHECK_EQ(schedule.refresh_millihz(), 144000u);
  CHECK_EQ(schedule.sequence(), 3u);
  CHECK_EQ(schedule.last(), last);
  CHECK_EQ(schedule.next(), last + 6944444u);
  CHECK_EQ(schedule.advance(last + 6944444u), 1u);
  CHECK_EQ(schedule.sequence(), 4u);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, a_new_rate_whose_first_blank_already_passed_runs_from_now)
{ VblankSchedule schedule;
  schedule.start(start_at);
  const std::uint64_t now = start_at + 10u * 16666667u;   /* ten blanks due, none counted */
  schedule.retime(1000000u, now);   /* 1 kHz: its first blank after the last one is long past */
  CHECK_EQ(schedule.sequence(), 0u);
  CHECK_EQ(schedule.last(), now);
  CHECK_EQ(schedule.next(), now + 1000000u);
  CHECK_EQ(schedule.advance(now + 999999u), 0u);
  CHECK_EQ(schedule.advance(now + 1000000u), 1u);
  schedule.retime(1000u, now + 1500000u);   /* 1 Hz: its first blank is still to come */
  CHECK_EQ(schedule.last(), now + 1000000u);
  CHECK_EQ(schedule.next(), now + 1000000u + second);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, a_host_that_slept_starts_again_rather_than_counting_forever)
{ VblankSchedule schedule;
  schedule.retime(1000000u, 0u);   /* 1 kHz: a millisecond a blank */
  schedule.start(start_at);
  const std::uint64_t woke = start_at + (VblankSchedule::most_overdue + 10u) * 1000000u;
  CHECK_EQ(schedule.advance(woke), VblankSchedule::most_overdue);
  CHECK_EQ(schedule.sequence(), VblankSchedule::most_overdue);
  CHECK_EQ(schedule.last(), woke);
  CHECK_EQ(schedule.next(), woke + 1000000u);
  CHECK_EQ(schedule.advance(woke + 999999u), 0u);
  CHECK(schedule.invariants_hold()); }

TEST(vblank_schedule, starting_again_keeps_the_count_and_moves_the_phase)
{ VblankSchedule schedule;
  schedule.start(start_at);
  CHECK_EQ(schedule.advance(start_at + 2u * 16666667u), 2u);
  schedule.start(7u * second);
  CHECK_EQ(schedule.sequence(), 2u);
  CHECK_EQ(schedule.last(), 7u * second);
  CHECK_EQ(schedule.next(), 7u * second + 16666666u);
  CHECK(schedule.invariants_hold()); }

namespace {

constexpr std::uint64_t period_60 = 16666666u;    /* 10^12 / 60000, rounded down */
constexpr std::uint64_t period_144 = 6944444u;    /* 10^12 / 144000, rounded down */

/* A schedule three 60 Hz blanks in, far enough from the clock's start that a
 * period before any blank is still a time. */
VblankSchedule three_blanks_in(std::uint64_t &last)
{
    VblankSchedule schedule;
    schedule.start(10u * second);
    CHECK_EQ(schedule.advance(10u * second + 3u * 16666667u), 3u);
    last = schedule.last();
    return schedule;
}

}  // namespace

/* The host's display is at 144 Hz and blanks 123 ns past a whole number of its
 * periods from the guest's last blank: the guest's next blank is the first
 * such one, and every one after it a host period apart. */
TEST(vblank_schedule, aligned_the_next_blank_is_the_first_in_phase_with_the_host_s)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(144000u, last + 10u * period_144 + 123u, last + 1000u);
  CHECK_EQ(schedule.refresh_millihz(), 144000u);
  CHECK_EQ(schedule.sequence(), 3u);
  CHECK_EQ(schedule.last(), last);   /* the blank that was, still */
  CHECK_EQ(schedule.next(), last + period_144 + 123u);
  CHECK_EQ(schedule.advance(last + period_144 + 122u), 0u);
  CHECK_EQ(schedule.advance(last + period_144 + 123u), 1u);
  CHECK_EQ(schedule.last(), last + period_144 + 123u);
  CHECK_EQ(schedule.next(), last + 2u * period_144 + 123u);
  CHECK(schedule.invariants_hold()); }

/* A host blank long past says the phase as well as one to come. */
TEST(vblank_schedule, aligned_to_a_blank_long_past_the_phase_is_the_same)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(60000u, last - 5u * period_60 + 1u, last + 2u);
  CHECK_EQ(schedule.next(), last + period_60 + 1u);
  CHECK_EQ(schedule.sequence(), 3u);
  CHECK(schedule.invariants_hold()); }

/* In phase a millisecond after the last blank would be a blank a millisecond
 * long: the guest gets the one a period later instead. */
TEST(vblank_schedule, aligned_no_blank_comes_sooner_than_half_a_period_after_the_last)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(60000u, last + 1000000u, last + 10u);
  CHECK_EQ(schedule.next(), last + 1000000u + period_60);
  /* Exactly half a period after it is soon enough. */
  schedule.align(60000u, last + period_60 / 2u - 2u * period_60, last + 10u);
  CHECK_EQ(schedule.next(), last + period_60 / 2u);
  CHECK(schedule.invariants_hold()); }

/* Aligned late in a period, the next blank is still to come, never one already
 * due: the guest is not handed a blank that is in the past. */
TEST(vblank_schedule, aligned_late_the_next_blank_is_after_now)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  const std::uint64_t now = last + 12000000u;
  schedule.align(60000u, last + 2000000u, now);
  CHECK_EQ(schedule.next(), last + 2000000u + period_60);
  CHECK(schedule.next() > now);
  CHECK_EQ(schedule.advance(now), 0u);
  CHECK(schedule.invariants_hold()); }

/* Twice before the next blank: the second measures from the blank that was,
 * not from where the first put the grid. */
TEST(vblank_schedule, aligned_twice_before_a_blank_still_measures_from_the_last_one)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(60000u, last + 9000000u, last + 10u);
  CHECK_EQ(schedule.next(), last + 9000000u);
  schedule.align(60000u, last + 1000000u, last + 20u);
  CHECK_EQ(schedule.last(), last);
  CHECK_EQ(schedule.next(), last + 1000000u + period_60);
  CHECK(schedule.invariants_hold()); }

/* Before it starts there is no blank to measure from: the rate is kept, and
 * start puts the first blank a period of it after the start. */
TEST(vblank_schedule, aligned_before_it_starts_only_the_rate_is_kept)
{ VblankSchedule schedule;
  schedule.align(144000u, 123456789u, 0u);
  CHECK_EQ(schedule.refresh_millihz(), 144000u);
  CHECK_EQ(schedule.sequence(), 0u);
  CHECK_EQ(schedule.advance(second), 0u);
  schedule.start(second);
  CHECK_EQ(schedule.next(), second + period_144);
  CHECK(schedule.invariants_hold()); }
