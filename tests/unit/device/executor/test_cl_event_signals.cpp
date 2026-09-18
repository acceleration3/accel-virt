/* The one place in the executor library another thread reaches.
 *
 * OpenCL says an event completed by calling a function on whatever thread the
 * host driver please, and it cannot be told to stop: a callback can arrive
 * after the session shut down, after the executor was destroyed, and after the
 * context is gone. What matters here is that such a callback does nothing at
 * all - not that it is unlikely, that it is harmless - and that a callback
 * that is still current reaches the device exactly once.
 *
 * The registrations are driven directly rather than through the host's OpenCL,
 * because what is being tested is what happens when the host calls back at the
 * worst possible moment, which no real driver can be asked to do on cue.
 * Every case below reaches ClEventSignals::signal_from_host, which is the
 * whole of what a foreign thread may do here: compare a generation, and call
 * the device. */
#include "device/executor/cl_event_registration.hpp"
#include "device/executor/cl_event_signals.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ClEventRegistration;
using av::ClEventSignals;

namespace {

/* What the device does with a signal: wake a worker. Here, count. */
struct Woken {
    uint32_t count = 0u;
    void    *context = nullptr;
};

Woken woken;

void wake(void *context)
{
    ++woken.count;
    woken.context = context;
}

/* A second device, to prove the one that is bound is the one called. */
Woken other;

void wake_other(void *context)
{
    ++other.count;
    other.context = context;
}

void forget_wakes()
{
    woken.count = 0u;
    woken.context = nullptr;
    other.count = 0u;
    other.context = nullptr;
}

int device = 0;
int another_device = 0;

}  // namespace

TEST(cl_event_signals, a_new_collection_signals_nothing_and_is_bound_to_nothing)
{ ClEventSignals signals;
  CHECK(!signals.bound());
  CHECK_EQ(signals.outstanding(), 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK_EQ(signals.stale(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_registration_that_completes_wakes_the_device_once)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  CHECK(signals.bound());
  ClEventRegistration *const registration = signals.take();
  CHECK(registration != nullptr);
  CHECK_EQ(signals.outstanding(), 1u);
  CHECK(signals.invariants_hold());
  registration->completed();
  CHECK_EQ(woken.count, 1u);
  CHECK(woken.context == &device);
  CHECK_EQ(signals.signalled(), 1u);
  CHECK_EQ(signals.stale(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, the_device_bound_last_is_the_one_woken)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  signals.bind(&wake_other, &another_device);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(other.count, 1u);
  CHECK(other.context == &another_device);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_callback_after_shutting_down_does_nothing_at_all)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  /* The session ends while the host still holds this. */
  signals.shut_down();
  CHECK(!signals.bound());
  CHECK_EQ(signals.outstanding(), 0u);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_callback_from_a_session_before_this_one_does_nothing)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const older = signals.take();
  signals.shut_down();
  /* The slot is reused by the next session on this backend, which is what a
   * pool of sessions does. The older registration must not be woken by, or
   * wake anything for, the newer one. */
  signals.bind(&wake, &device);
  ClEventRegistration *const newer = signals.take();
  CHECK(older == newer);
  older->completed();
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 0u);
  /* And the generation the older one carried is gone for good: a second
   * shut_down moves past it again. */
  signals.shut_down();
  newer->completed();
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_registration_given_back_no_longer_signals)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  /* The host refused the registration, so the slot goes back. */
  signals.give_back(registration);
  CHECK_EQ(signals.outstanding(), 0u);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, giving_back_what_was_never_taken_changes_nothing)
{ ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  signals.give_back(registration);
  signals.give_back(registration);
  signals.give_back(nullptr);
  CHECK_EQ(signals.outstanding(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_completion_with_no_device_bound_wakes_nobody)
{ forget_wakes();
  ClEventSignals signals;
  /* Version 4 devices bind nothing. A registration is still refused before it
   * is made - the backend asks `bound` first - but a collection that is asked
   * anyway must not call through a null. */
  ClEventRegistration *const registration = signals.take();
  CHECK(registration != nullptr);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_collection_that_is_full_refuses_and_stays_usable)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *first = nullptr;
  for (uint32_t index = 0u; index < ClEventSignals::max_registrations; ++index) {
      ClEventRegistration *const taken = signals.take();
      CHECK(taken != nullptr);
      if (index == 0u) first = taken;
  }
  CHECK_EQ(signals.outstanding(), ClEventSignals::max_registrations);
  CHECK(signals.take() == nullptr);
  CHECK(signals.invariants_hold());
  /* One completing frees nothing - the host may call a callback once and the
   * registration stays until the session ends - but one given back does. */
  signals.give_back(first);
  CHECK(signals.take() != nullptr);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, every_registration_of_a_full_collection_signals)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *taken[ClEventSignals::max_registrations] = {};
  for (uint32_t index = 0u; index < ClEventSignals::max_registrations; ++index) {
      taken[index] = signals.take();
  }
  for (uint32_t index = 0u; index < ClEventSignals::max_registrations; ++index) {
      taken[index]->completed();
  }
  CHECK_EQ(woken.count, ClEventSignals::max_registrations);
  CHECK_EQ(signals.signalled(), ClEventSignals::max_registrations);
  /* And every one of them goes stale together. */
  signals.shut_down();
  for (uint32_t index = 0u; index < ClEventSignals::max_registrations; ++index) {
      taken[index]->completed();
  }
  CHECK_EQ(woken.count, ClEventSignals::max_registrations);
  CHECK_EQ(signals.stale(), ClEventSignals::max_registrations);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_registration, one_that_belongs_to_nothing_signals_nothing)
{ ClEventRegistration registration;
  CHECK(!registration.armed());
  CHECK_EQ(registration.generation(), 0u);
  CHECK(registration.invariants_hold());
  /* Nothing bound it, so there is nobody to tell. */
  registration.completed();
  CHECK(registration.invariants_hold()); }

TEST(cl_event_registration, arming_carries_the_generation_it_was_armed_at)
{ ClEventSignals signals;
  ClEventRegistration registration;
  registration.bind(signals);
  registration.arm(7u);
  CHECK(registration.armed());
  CHECK_EQ(registration.generation(), 7u);
  CHECK(registration.invariants_hold());
  registration.disarm();
  CHECK(!registration.armed());
  CHECK_EQ(registration.generation(), 0u);
  CHECK(registration.invariants_hold()); }
