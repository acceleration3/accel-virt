/* A head's blanks: the first one period after the schedule starts, each after
 * that at exactly n periods - the rate's fraction of a nanosecond carried, so
 * sixty blanks at 60 Hz are one second to the nanosecond - every blank that
 * passed counted however late the count comes, and a new rate taking over
 * from the last blank without the sequence moving back or skipping. */
#include "device/display/vblank_schedule.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <initializer_list>

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

constexpr std::uint32_t hz_60 = 60000000u;     /* in microhertz */
constexpr std::uint32_t hz_144 = 144000000u;

/* The shift is the consumer's against the grid it saw: the next blank is the
 * one the old rate had due, `shift` earlier, and the new rate carries on from
 * there. 144 Hz with the blank 123 ns early is the 60 Hz blank less 123 ns,
 * then every one after it a 144 Hz period apart. */
TEST(vblank_schedule, aligned_the_next_blank_is_the_one_due_less_the_shift_and_the_new_rate_carries_on)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  const std::uint64_t was = schedule.next();
  schedule.align(hz_144, 123, last + 1000u);
  CHECK_EQ(schedule.refresh_millihz(), 144000u);
  CHECK_EQ(schedule.sequence(), 3u);
  CHECK_EQ(schedule.last(), last);   /* the blank that was, still */
  CHECK_EQ(schedule.next(), was - 123u);
  CHECK_EQ(schedule.advance(was - 124u), 0u);
  CHECK_EQ(schedule.advance(was - 123u), 1u);
  CHECK_EQ(schedule.last(), was - 123u);
  CHECK_EQ(schedule.next(), was - 123u + period_144);
  CHECK(schedule.invariants_hold()); }

/* A negative shift puts the blank later. */
TEST(vblank_schedule, aligned_a_negative_shift_is_later_and_none_is_the_blank_that_was_due)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  const std::uint64_t was = schedule.next();
  CHECK_EQ(was, last + period_60);
  schedule.align(hz_60, -3000000, last + 10u);
  CHECK_EQ(schedule.next(), was + 3000000u);
  CHECK_EQ(schedule.sequence(), 3u);
  VblankSchedule unshifted = three_blanks_in(last);
  unshifted.align(hz_60, 0, last + 10u);
  CHECK_EQ(unshifted.next(), was);
  VblankSchedule earlier = three_blanks_in(last);
  earlier.align(hz_60, 2000000, last + 10u);
  CHECK_EQ(earlier.next(), was - 2000000u);
  CHECK(schedule.invariants_hold()); }

/* A zero shift at the rate the schedule has is no retiming: every blank after
 * it is where it was to the nanosecond or two the rounding of a new epoch
 * costs, on a rate whose period is not a whole nanosecond and one that has
 * run a while. */
TEST(vblank_schedule, aligned_by_nothing_at_the_same_rate_leaves_the_blanks_where_they_were)
{ for (const std::uint32_t microhz : {hz_60, hz_144, 59999460u}) {
      std::uint64_t last = 0u;
      VblankSchedule kept = three_blanks_in(last);
      kept.align(microhz, 0, last + 10u);
      static_cast<void>(kept.advance(kept.next() + 100u * second / 60u));
      VblankSchedule moved = kept;
      moved.align(microhz, 0, kept.last() + 10u);
      CHECK_EQ(moved.last(), kept.last());
      CHECK_EQ(moved.sequence(), kept.sequence());
      for (std::uint32_t blank = 0u; blank < 600u; blank++) {
          const std::uint64_t was = kept.next();
          const std::uint64_t is = moved.next();
          CHECK((was > is ? was - is : is - was) <= 1u);
          CHECK_EQ(kept.advance(was), 1u);
          CHECK_EQ(moved.advance(is), 1u);
      }
      CHECK(moved.invariants_hold());
  } }

/* A shift that would put the blank within half a period of the last one puts
 * it a period later instead; exactly half a period after it is soon enough. */
TEST(vblank_schedule, aligned_no_blank_comes_sooner_than_half_a_period_after_the_last)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(hz_60, 9000000, last + 10u);
  CHECK_EQ(schedule.next(), last + 2u * period_60 - 9000000u);
  schedule = three_blanks_in(last);
  schedule.align(hz_60, static_cast<std::int64_t>(period_60 - period_60 / 2u), last + 10u);
  CHECK_EQ(schedule.next(), last + period_60 / 2u);
  schedule = three_blanks_in(last);
  schedule.align(hz_60, static_cast<std::int64_t>(period_60 - period_60 / 2u + 1u), last + 10u);
  CHECK_EQ(schedule.next(), last + period_60 / 2u + period_60 - 1u);
  /* A shift of most of a second is many periods early, and still no closer. */
  schedule = three_blanks_in(last);
  schedule.align(hz_60, 900000000, last + 10u);
  CHECK(schedule.next() >= last + period_60 / 2u);
  CHECK(schedule.next() < last + period_60 / 2u + period_60);
  CHECK_EQ((schedule.next() + 900000000u - last) % period_60, 0u);
  CHECK(schedule.invariants_hold()); }

/* The floors are the new rate's: moving to a rate whose half period is longer
 * than the shifted blank's distance from the last one moves it on by whole
 * periods of the new rate. */
TEST(vblank_schedule, aligned_to_a_slower_rate_the_floor_is_half_of_its_period)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  const std::uint64_t was = schedule.next();
  const std::uint64_t period_30 = 33333333u;
  schedule.align(30000000u, 10000000, last + 10u);
  CHECK(was - 10000000u < last + period_30 / 2u);
  CHECK_EQ(schedule.next(), was - 10000000u + period_30);
  CHECK(schedule.invariants_hold()); }

/* Aligned late in a period, the next blank is still to come, never one already
 * due: the guest is not handed a blank that is in the past, and the blank it
 * gets keeps the phase of the shift. */
TEST(vblank_schedule, aligned_late_the_next_blank_is_after_now_by_whole_periods)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  const std::uint64_t now = last + 12000000u;
  schedule.align(hz_60, 2000000, now);
  CHECK_EQ(schedule.next(), last + period_60 - 2000000u);   /* 14.7 ms: still to come */
  CHECK_EQ(schedule.advance(now), 0u);
  schedule = three_blanks_in(last);
  schedule.align(hz_60, 2000000, last + 20000000u);
  CHECK_EQ(schedule.next(), last + 2u * period_60 - 2000000u);
  schedule = three_blanks_in(last);
  schedule.align(hz_60, 2000000, last + 100000000u);
  CHECK(schedule.next() > last + 100000000u);
  CHECK(schedule.next() <= last + 100000000u + period_60);
  CHECK_EQ((schedule.next() + 2000000u - last) % period_60, 0u);
  /* Exactly at the blank is not after it. */
  schedule = three_blanks_in(last);
  schedule.align(hz_60, 0, last + period_60);
  CHECK_EQ(schedule.next(), last + 2u * period_60);
  CHECK(schedule.invariants_hold()); }

/* Early by more than the time since the clock began keeps the phase. */
TEST(vblank_schedule, aligned_early_by_more_than_the_clock_has_run_keeps_the_phase)
{ VblankSchedule schedule;
  schedule.start(1000u);
  schedule.align(hz_60, 900000000, 1000u);
  CHECK(schedule.next() > 1000u);
  CHECK(schedule.next() >= 1000u + period_60 / 2u);
  CHECK(schedule.next() < 1000u + period_60 / 2u + period_60);
  CHECK_EQ((schedule.next() + 900000000u - 1000u) % period_60, 0u);
  CHECK(schedule.invariants_hold()); }

/* Twice before the next blank: the second is measured against the grid the
 * first made, which is the one the consumer's frames are then judged by. */
TEST(vblank_schedule, aligned_twice_before_a_blank_the_second_shift_is_from_where_the_first_put_it)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(hz_60, 5000000, last + 10u);
  CHECK_EQ(schedule.next(), last + period_60 - 5000000u);
  schedule.align(hz_60, 1000000, last + 20u);
  CHECK_EQ(schedule.last(), last);
  CHECK_EQ(schedule.next(), last + period_60 - 6000000u);
  CHECK(schedule.invariants_hold()); }

/* A rate off the millihertz grid holds: a thousand seconds of blanks at
 * 59.99946 Hz end within a nanosecond or two of where that many exact periods
 * put the last one, where the rate rounded to millihertz would be 7.7 us out. */
TEST(vblank_schedule, a_rate_between_millihertz_holds_over_a_thousand_seconds)
{ constexpr std::uint32_t microhz = 59999460u;
  std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(microhz, 0, last + 10u);
  const std::uint64_t first = schedule.next();
  const std::uint64_t passed = schedule.advance(first + 1000u * second);
  const long double   exact_period = 1e15L / microhz;
  const long double   exact = static_cast<long double>(first) + static_cast<long double>(passed - 1u) * exact_period;
  const long double   error = static_cast<long double>(schedule.last()) - exact;
  CHECK(passed > 59999u);
  CHECK(error < 2.0L && error > -2.0L);
  CHECK(schedule.invariants_hold()); }

/* The page says the rate in whole millihertz, the nearest one. */
TEST(vblank_schedule, the_rate_is_reported_rounded_to_the_nearest_millihertz)
{ std::uint64_t last = 0u;
  VblankSchedule schedule = three_blanks_in(last);
  schedule.align(59999499u, 0, last + 10u);
  CHECK_EQ(schedule.refresh_millihz(), 59999u);
  schedule.align(59999500u, 0, last + 10u);
  CHECK_EQ(schedule.refresh_millihz(), 60000u);
  schedule.align(1000000u, 0, last + 10u);
  CHECK_EQ(schedule.refresh_millihz(), 1000u);
  schedule.align(1000000000u, 0, last + 10u);
  CHECK_EQ(schedule.refresh_millihz(), 1000000u);
  CHECK(schedule.invariants_hold()); }

/* Before it starts there is no blank to measure from: the rate is kept, and
 * start puts the first blank a period of it after the start. */
TEST(vblank_schedule, aligned_before_it_starts_only_the_rate_is_kept)
{ VblankSchedule schedule;
  schedule.align(hz_144, 123456789, 0u);
  CHECK_EQ(schedule.refresh_millihz(), 144000u);
  CHECK_EQ(schedule.sequence(), 0u);
  CHECK_EQ(schedule.advance(second), 0u);
  schedule.start(second);
  CHECK_EQ(schedule.next(), second + period_144);
  CHECK(schedule.invariants_hold()); }
