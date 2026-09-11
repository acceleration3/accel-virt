#include "core/platform/periodic_timer.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::Irql;
using av::MockPlatform;
using av::PeriodicTimer;
using av::Status;
using av::Violation;

namespace {

void count(void *, void *context) { ++*static_cast<unsigned *>(context); }

}  // namespace

TEST(periodic_timer_initialize, creates_a_stopped_timer)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  CHECK(!timer.created());
  CHECK_STATUS(timer.initialize(), Status::ok);
  CHECK(timer.created());
  platform.run_timers(1000u);
  CHECK_EQ(ticks, 0u);
  CHECK_STATUS(timer.initialize(), Status::invalid_state); }

TEST(periodic_timer_initialize, out_of_memory_leaves_no_timer)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(timer.initialize(), Status::no_memory);
  CHECK(!timer.created()); }

TEST(periodic_timer_start, ticks_every_period_until_stopped)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  CHECK_STATUS(timer.initialize(), Status::ok);
  CHECK_STATUS(timer.start(100u), Status::ok);
  platform.run_timers(350u);
  CHECK_EQ(ticks, 3u);
  timer.stop();
  platform.run_timers(350u);
  CHECK_EQ(ticks, 3u); }

TEST(periodic_timer_start, needs_a_timer_and_a_period)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  CHECK_STATUS(timer.start(100u), Status::invalid_state);
  timer.stop();
  CHECK_STATUS(timer.initialize(), Status::ok);
  CHECK_STATUS(timer.start(0u), Status::invalid_parameter); }

TEST(periodic_timer_destroy, deletes_the_timer_once)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  timer.destroy();
  CHECK_STATUS(timer.initialize(), Status::ok);
  CHECK_STATUS(timer.start(10u), Status::ok);
  timer.destroy();
  CHECK(!timer.created());
  platform.run_timers(100u);
  CHECK_EQ(ticks, 0u);
  timer.destroy();
  CHECK_STATUS(timer.initialize(), Status::ok); }

TEST(periodic_timer, start_and_stop_are_dispatch_the_rest_passive)
{ MockPlatform platform;
  unsigned ticks = 0u;
  PeriodicTimer timer(platform, &count, &ticks);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(timer.initialize());
  static_cast<void>(timer.start(10u));
  timer.stop();
  timer.destroy();
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }
