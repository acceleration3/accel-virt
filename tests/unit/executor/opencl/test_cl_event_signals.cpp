#include "executor/opencl/cl_event_registration.hpp"
#include "executor/opencl/cl_event_signals.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::ClEventRegistration;
using av::ClEventRegistrationProbe;
using av::ClEventSignals;
using av::ClEventSignalsProbe;

namespace av {
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
    { return *signals.slots_.at(index); }
    static std::uint32_t made(const ClEventSignals &signals) { return signals.slots_.slots(); }
    static void miscount(ClEventSignals &signals) { signals.taken_ += 1u; }
    static void an_epoch_of_nothing(ClEventSignals &signals) { signals.generation_ = 0u; }
    static std::uint64_t epoch_of(const ClEventSignals &signals) { return signals.generation_; }
};

}

namespace {
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

constexpr std::uint32_t many = 500u;

}

TEST(cl_event_signals, a_new_collection_signals_nothing_and_is_bound_to_nothing)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  CHECK(!signals.bound());
  CHECK_EQ(signals.outstanding(), 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK_EQ(signals.stale(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_registration_that_completes_wakes_the_device_once)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
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

TEST(cl_event_signals, a_completed_registration_gives_its_slot_back)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *held[many] = {};
  for (ClEventRegistration *&registration : held) registration = signals.take();
  CHECK(held[many - 1u] != nullptr);
  CHECK_EQ(signals.outstanding(), many);
  CHECK(signals.invariants_hold());
  for (ClEventRegistration *registration : held) registration->completed();
  CHECK_EQ(signals.outstanding(), 0u);
  CHECK_EQ(signals.signalled(), many);
  CHECK(!held[0]->armed());
  CHECK(signals.invariants_hold());
  held[0]->completed();
  CHECK_EQ(signals.stale(), 1u);
  CHECK_EQ(signals.outstanding(), 0u);
  for (ClEventRegistration *&registration : held) registration = signals.take();
  CHECK(held[many - 1u] != nullptr);
  CHECK_EQ(signals.outstanding(), many);
  CHECK_EQ(ClEventSignalsProbe::made(signals), many);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, the_device_bound_last_is_the_one_woken)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
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
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
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
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const older = signals.take();
  signals.shut_down();
  signals.bind(&wake, &device);
  ClEventRegistration *const newer = signals.take();
  CHECK(older == newer);
  older->completed();
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 0u);
  signals.shut_down();
  newer->completed();
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_registration_given_back_no_longer_signals)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  signals.give_back(registration);
  CHECK_EQ(signals.outstanding(), 0u);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, giving_back_what_was_never_taken_changes_nothing)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const registration = signals.take();
  signals.give_back(registration);
  signals.give_back(registration);
  signals.give_back(nullptr);
  CHECK_EQ(signals.outstanding(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_completion_with_no_device_bound_wakes_nobody)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  ClEventRegistration *const registration = signals.take();
  CHECK(registration != nullptr);
  registration->completed();
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, a_collection_that_cannot_grow_refuses_and_stays_usable)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *first = signals.take();
  CHECK(first != nullptr);
  platform.fail_allocation_at(1u);
  std::uint32_t taken = 1u;
  while (signals.take() != nullptr) taken++;
  CHECK_EQ(signals.outstanding(), taken);
  CHECK(signals.invariants_hold());
  signals.give_back(first);
  CHECK(signals.take() == first);
  CHECK(first->armed());
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, every_registration_of_a_full_collection_signals)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *taken[many] = {};
  for (uint32_t index = 0u; index < many; ++index) {
      taken[index] = signals.take();
  }
  for (uint32_t index = 0u; index < many; ++index) {
      taken[index]->completed();
  }
  CHECK_EQ(woken.count, many);
  CHECK_EQ(signals.signalled(), many);
  signals.shut_down();
  for (uint32_t index = 0u; index < many; ++index) {
      taken[index]->completed();
  }
  CHECK_EQ(woken.count, many);
  CHECK_EQ(signals.stale(), many);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_registration, one_that_belongs_to_nothing_signals_nothing)
{ ClEventRegistration registration;
  CHECK(!registration.armed());
  CHECK_EQ(registration.generation(), 0u);
  CHECK(registration.invariants_hold());
  registration.completed();
  CHECK(registration.invariants_hold()); }

TEST(cl_event_registration, arming_carries_the_generation_it_was_armed_at)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
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

TEST(cl_event_signals, a_callback_naming_no_epoch_at_all_does_nothing)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  signals.signal_from_host(0u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.signalled(), 0u);
  CHECK(signals.invariants_hold()); }

TEST(cl_event_signals, the_invariant_catches_a_count_that_disagrees_with_what_is_armed)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  CHECK(signals.invariants_hold());
  ClEventSignalsProbe::miscount(signals);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, the_invariant_catches_a_registration_armed_for_another_session)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  CHECK(signals.invariants_hold());
  ClEventRegistrationProbe::another_sessions_epoch(*taken);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, the_invariant_catches_a_slot_disarmed_with_its_epoch_left_behind)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  ClEventRegistrationProbe::disarm(*taken);
  CHECK(!signals.invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, the_invariant_catches_an_epoch_of_nothing)
{ av::MockPlatform platform;
  ClEventSignals   signals(platform);
  CHECK(signals.invariants_hold());
  ClEventSignalsProbe::an_epoch_of_nothing(signals);
  CHECK(!signals.invariants_hold()); }

TEST(cl_event_signals, a_registrations_invariant_catches_an_owner_and_an_epoch_disagreeing)
{ ClEventRegistration idle;
  CHECK(idle.invariants_hold());
  ClEventRegistrationProbe::an_epoch_with_no_owner(idle);
  CHECK(!idle.invariants_hold());

  ClEventRegistration armed;
  ClEventRegistrationProbe::armed_with_no_owner(armed);
  CHECK(!armed.invariants_hold());

  av::MockPlatform           platform;
  ClEventSignals             signals(platform);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  ClEventRegistrationProbe::an_epoch_it_is_not_armed_for(*taken);
  CHECK(!taken->invariants_hold());
  signals.shut_down(); }

TEST(cl_event_signals, a_callback_naming_another_sessions_epoch_wakes_nobody)
{ forget_wakes();
  av::MockPlatform platform;
  ClEventSignals   signals(platform);
  signals.bind(&wake, &device);
  ClEventRegistration *const taken = signals.take();
  CHECK(taken != nullptr);
  signals.signal_from_host(ClEventSignalsProbe::epoch_of(signals) + 1u);
  CHECK_EQ(woken.count, 0u);
  CHECK_EQ(signals.stale(), 1u);
  signals.signal_from_host(ClEventSignalsProbe::epoch_of(signals));
  CHECK_EQ(woken.count, 1u);
  CHECK_EQ(signals.stale(), 1u);
  CHECK(signals.invariants_hold());
  signals.shut_down(); }
