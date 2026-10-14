/* What a control exchange sleeps on while the host works on its request: set
 * from the DPC, waited on at PASSIVE, and unset by the wait it ends. */
#include "core/platform/wake_event.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::Irql;
using av::MockPlatform;
using av::Violation;
using av::WakeEvent;

TEST(wake_event, reserves_storage_for_the_kernel_event)
{ CHECK(WakeEvent::storage_bytes >= 24u); }

/* A set before the wait - the answer harvested before the waiter slept - wakes
 * it at once, and only once. */
TEST(wake_event_wait, a_set_event_wakes_at_once_and_is_unset_by_the_wake)
{ MockPlatform platform; WakeEvent event(platform);
  platform.set_irql(Irql::dispatch);
  event.set();
  platform.set_irql(Irql::passive);
  const std::uint64_t before = platform.interrupt_time();
  CHECK(event.wait(1000u));
  CHECK_EQ(platform.interrupt_time(), before);
  CHECK(!event.wait(1000u));
  CHECK_EQ(platform.interrupt_time(), before + 1000u);
  CHECK_EQ(platform.slept_ticks(), 1000u);
  CHECK_EQ(platform.stalled_ticks(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

namespace {

struct Setter {
    WakeEvent    *event;
    MockPlatform *platform;
    std::uint32_t calls = 0u;
};

}  // namespace

/* What happens while the thread sleeps happens on the device's side: a set
 * then - the DPC finding the answer - ends the wait without the clock moving
 * past it; nothing set times the wait out. */
TEST(wake_event_wait, the_device_acts_while_the_thread_sleeps)
{ MockPlatform platform; WakeEvent event(platform);
  Setter setter{&event, &platform};
  platform.on_stall([](void *context) {
      auto *const state = static_cast<Setter *>(context);
      state->calls++;
      state->platform->set_irql(Irql::dispatch);
      state->event->set();
      state->platform->set_irql(Irql::passive);
  }, &setter);
  CHECK(event.wait(50000u));
  CHECK_EQ(setter.calls, 1u);
  CHECK_EQ(platform.interrupt_time(), 0u);
  platform.on_stall(nullptr, nullptr);
  CHECK(!event.wait(50000u));
  CHECK_EQ(platform.interrupt_time(), 50000u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(wake_event_wait, above_passive_breaks_its_ceiling_and_blocks_at_dispatch)
{ MockPlatform platform; WakeEvent event(platform);
  platform.set_irql(Irql::apc);
  CHECK(!event.wait(10u));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::dispatch);
  CHECK(!event.wait(10u));
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  platform.set_irql(Irql::passive); }

TEST(wake_event_set, above_dispatch_breaks_its_ceiling)
{ MockPlatform platform; WakeEvent event(platform);
  platform.set_irql(Irql::isr);
  event.set();
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive);
  CHECK(event.wait(10u)); }

TEST(wake_event_construct, above_dispatch_breaks_its_ceiling_and_starts_unset)
{ MockPlatform platform;
  platform.set_irql(Irql::isr);
  WakeEvent event(platform);
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive);
  CHECK(!event.wait(10u)); }
