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
using av::ClEventRegistrationProbe;
using av::ClEventSignals;
using av::ClEventSignalsProbe;

namespace av {

/* The states neither class is ever in. A registration is armed for exactly
 * one session, and the epoch is what says which - so one armed for another
 * session's, or one that holds an epoch while armed for nobody, is a callback
 * that would be believed when it should be ignored. */
struct ClEventRegistrationProbe {
    static void an_epoch_with_no_owner(ClEventRegistration &slot)
    { slot.owner_ = nullptr; slot.generation_ = 1u; }
    static void armed_with_no_owner(ClEventRegistration &slot)
    { slot.owner_ = nullptr; slot.armed_ = true; }
    static void an_epoch_it_is_not_armed_for(ClEventRegistration &slot)
    { slot.armed_ = false; slot.generation_ = 1u; }
    static void another_sessions_epoch(ClEventRegistration &slot) { slot.generation_ += 1u; }
    static void disarm(ClEventRegistration &slot) { slot.armed_ = false; }
};

struct ClEventSignalsProbe {
    static ClEventRegistration &slot(ClEventSignals &signals, std::uint32_t index)
    { return signals.slots_[index]; }
    static void miscount(ClEventSignals &signals) { signals.taken_ += 1u; }
    static void count_past_the_table(ClEventSignals &signals)
    { signals.taken_ = ClEventSignals::max_registrations + 1u; }
    static void an_epoch_of_nothing(ClEventSignals &signals) { signals.generation_ = 0u; }
    static std::uint64_t epoch_of(const ClEventSignals &signals) { return signals.generation_; }
};

}  // namespace av

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

/* A callback naming an epoch of nothing is ignored like any other stale one:
 * zero is what a registration that was never armed carries, and believing it
 * would be believing a callback from no session at all. */
TEST(cl_event_signals, a_callback_naming_no_epoch_at_all_does_nothing)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  signals.signal_from_host(0u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK(signals.invariants_hold()); }

/* The invariant catches the states the collection is never in. */
TEST(cl_event_signals, the_invariant_catches_a_count_that_disagrees_with_what_is_armed)
{ ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  CHECK(signals.invariants_hold());
  /* One more counted than armed: a slot that can never be handed out. */
  ClEventSignalsProbe::miscount(signals);
  CHECK(!signals.invariants_hold());
  ClEventSignalsProbe::count_past_the_table(signals);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, the_invariant_catches_a_registration_armed_for_another_session)
{ ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  CHECK(signals.invariants_hold());
  /* The epoch is what says which session a callback belongs to. One that
   * names another would be believed, and that session is gone. */
  ClEventRegistrationProbe::another_sessions_epoch(*taken);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, the_invariant_catches_a_slot_disarmed_with_its_epoch_left_behind)
{ ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  ClEventRegistrationProbe::disarm(*taken);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

/* A collection whose epoch is nothing has been shut down, so nothing it holds
 * can be armed for it and no callback may be believed. */
TEST(cl_event_signals, the_invariant_catches_an_epoch_of_nothing)
{ ClEventSignals signals;
  CHECK(signals.invariants_hold());
  ClEventSignalsProbe::an_epoch_of_nothing(signals);
  CHECK(!signals.invariants_hold()); }

/* One registration's own invariant: an owner and an epoch that disagree. */
TEST(cl_event_signals, a_registrations_invariant_catches_an_owner_and_an_epoch_disagreeing)
{ ClEventRegistration idle;
  CHECK(idle.invariants_hold());
  /* An epoch with nobody to belong to. */
  ClEventRegistrationProbe::an_epoch_with_no_owner(idle);
  CHECK(!idle.invariants_hold());

  ClEventRegistration armed;
  /* Armed for nobody, which is a callback with nowhere to go. */
  ClEventRegistrationProbe::armed_with_no_owner(armed);
  CHECK(!armed.invariants_hold());

  ClEventSignals             signals;
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  ClEventRegistrationProbe::an_epoch_it_is_not_armed_for(*taken);
  CHECK(!taken->invariants_hold());
  signals.shut_down(); }

/* A callback naming an epoch that is not this session's does nothing.
 *
 * OpenCL cannot unregister a callback, so one may fire after the session it
 * belonged to is gone and the slot has been handed to another. The epoch is
 * the only thing that tells them apart, and believing a stale one would wake
 * a device on behalf of a session that no longer exists. */
TEST(cl_event_signals, a_callback_naming_another_sessions_epoch_wakes_nobody)
{ forget_wakes();
  ClEventSignals signals;
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  /* An epoch that is not nothing and is not this one: the two cases the
   * guard separates, so the second has to be reached on its own. */
  signals.signal_from_host(ClEventSignalsProbe::epoch_of(signals) + 1u);
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.stale(), 1u);
  /* The one that is this session's still wakes. */
  signals.signal_from_host(ClEventSignalsProbe::epoch_of(signals));
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold());
  signals.shut_down(); }
