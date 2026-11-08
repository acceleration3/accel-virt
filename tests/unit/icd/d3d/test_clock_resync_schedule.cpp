#include "harness/framework/test_registry.hpp"
#include "icd/d3d/clock_resync_schedule.hpp"

#include <cstdint>

using av::d3d::ClockResyncSchedule;

namespace {
constexpr std::uint64_t interval = ClockResyncSchedule::interval_nanoseconds;
}

TEST(clock_resync_schedule, the_first_look_is_always_due)
{ ClockResyncSchedule schedule;
  CHECK(schedule.due(0u));
  CHECK(schedule.due(123u));
  CHECK(schedule.invariants_hold()); }

TEST(clock_resync_schedule, nothing_is_due_until_the_interval_has_passed_since_the_last_take)
{ ClockResyncSchedule schedule;
  schedule.taken(1000u);
  CHECK(!schedule.due(1000u));
  CHECK(!schedule.due(1000u + interval - 1u));
  CHECK(schedule.due(1000u + interval));
  CHECK(schedule.due(1000u + 10u * interval));
  schedule.taken(1000u + interval);
  CHECK(!schedule.due(1000u + interval + 1u));
  CHECK(schedule.invariants_hold()); }

TEST(clock_resync_schedule, a_clock_that_went_backwards_is_due_rather_than_waiting_for_ever)
{ ClockResyncSchedule schedule;
  schedule.taken(5u * interval);
  CHECK(schedule.due(interval));
  CHECK(!schedule.due(5u * interval + 1u)); }
