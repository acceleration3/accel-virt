#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/counting_event_owner.hpp"
#include "core/support/growable_array.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_object.hpp"
#include "icd/opencl/cl_watched_event.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <cstdint>

using av::ClContextRig;
using av::CountingEventOwner;
using av::ScriptedThread;
using av::Status;
using av::clicd::ClEventWatcher;
using av::clicd::ClWatchedEvent;
using av::clicd::ClWatchedEventProbe;
using av::clicd::ClObject;
using av::clicd::GuestClContext;

namespace av {
struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};
}

namespace av::clicd {
struct ClWatchedEventProbe {
    static void forget_event(ClWatchedEvent &watched) { watched.event_ = nullptr; }
    static void forget_only_the_notify(ClWatchedEvent &watched) { watched.notify_ = nullptr; }
    static void forget_transport(ClWatchedEvent &watched) { watched.transport_ = nullptr; }
    static void forget_notify(ClWatchedEvent &watched) { watched.notify_ = nullptr; }
    static void forget_wire(ClWatchedEvent &watched) { watched.wire_ = 0u; }
    static void stray_event(ClWatchedEvent &watched, ClObject *event) { watched.event_ = event; }
    static void stray_transport(ClWatchedEvent &watched, ClientTransport *wire)
    { watched.transport_ = wire; }
    static void stray_notify(ClWatchedEvent &watched, ClWatchedEvent::Notify notify)
    { watched.notify_ = notify; }
    static void stray_wire(ClWatchedEvent &watched) { watched.wire_ = 1u; }
};

}

namespace {
struct Ran {
    std::uint32_t times = 0u;
    cl_int        status = CL_QUEUED;
    void         *user_data = nullptr;
    cl_event      event = nullptr;
};

Ran ran;

void CL_CALLBACK note(cl_event event, cl_int status, void *user_data)
{
    ++ran.times;
    ran.status = status;
    ran.user_data = user_data;
    ran.event = event;
}

void forget()
{
    ran.times = 0u;
    ran.status = CL_QUEUED;
    ran.user_data = nullptr;
    ran.event = nullptr;
}

int user_data = 0;

cl_icd_dispatch dispatch = {};

class Watching {
public:
    Watching()
        : rig_(), thread_(), owner_(), watcher_(rig_.platform(), thread_, rig_.lines(), rig_.send_lock(), owner_),
          event_(dispatch, rig_.platform(), ClObject::Kind::event, 0x6900u, nullptr, nullptr)
    {
        rig_.host().arena_import(0x77u, 3u);
        static_cast<void>(
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.blocks_lock(), rig_.lines(), rig_.handles()));
        CHECK_STATUS(context_.object().make(), Status::ok);
    }
    ~Watching() { context_.destroy(); }

    ClEventWatcher   &watcher() { return watcher_; }
    ScriptedThread   &thread() { return thread_; }
    ClContextRig     &rig() { return rig_; }
    ClObject         &event() { return event_; }
    CountingEventOwner &owner() { return owner_; }
    av::ClientTransport *wire() { return context_.object().wire(); }

private:
    ClContextRig                   rig_;
    ScriptedThread                 thread_;
    CountingEventOwner             owner_;
    ClEventWatcher                 watcher_;
    ClObject                       event_;
    av::ObjectStorage<GuestClContext> context_;
};

void event_is(ClContextRig &rig, cl_int status)
{
    rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&status), sizeof status);
}

}

TEST(cl_event_watcher, a_watcher_with_nothing_registered_runs_no_thread)
{ Watching rig;
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(!rig.thread().running());
  CHECK_EQ(rig.thread().starts(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, registering_the_first_callback_starts_the_thread)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.thread().running());
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.watcher().registered(), 2u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_callback_runs_once_when_its_event_completes)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);

  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.watcher().registered(), 1u);

  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, CL_COMPLETE);
  CHECK(ran.user_data == &user_data);
  CHECK(ran.event == reinterpret_cast<cl_event>(&rig.event()));
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.watcher().delivered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, the_loop_rests_when_nothing_is_left_and_a_registration_wakes_it)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.thread().rests(), 0u);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().rests(), 1u);
  CHECK(rig.thread().running());
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.thread().wakes(), 1u);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 2u);
  CHECK_EQ(rig.thread().rests(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, an_event_that_ended_in_an_error_runs_its_callback_too)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  event_is(rig.rig(), -5);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, -5); }

TEST(cl_event_watcher, a_callback_registered_for_an_earlier_status_runs_at_that_status)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_RUNNING, &note, &user_data, false),
           CL_SUCCESS);
  event_is(rig.rig(), CL_SUBMITTED);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, CL_RUNNING); }

TEST(cl_event_watcher, a_registration_of_nothing_is_refused)
{ Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, nullptr, nullptr, false),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(nullptr, 0x6900u, rig.wire(), CL_COMPLETE, &note, nullptr, false),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0u, rig.wire(), CL_COMPLETE, &note, nullptr, false),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, nullptr, CL_COMPLETE, &note, nullptr, false),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(!rig.thread().running());
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_watcher_that_cannot_grow_refuses_and_keeps_what_it_has)
{ Watching rig;
  constexpr std::uint32_t room = av::GrowableArray<char>::first_capacity;
  for (std::uint32_t index = 0u; index < room; ++index) {
      CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u + index, rig.wire(), CL_COMPLETE, &note,
                                   nullptr, false),
               CL_SUCCESS);
  }
  CHECK_EQ(rig.watcher().registered(), room);
  rig.rig().platform().fail_allocation_at(1u);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x7000u, rig.wire(), CL_COMPLETE, &note, nullptr, false),
           CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.watcher().registered(), room);
  CHECK(rig.watcher().invariants_hold());
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x7000u, rig.wire(), CL_COMPLETE, &note, nullptr, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), room + 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_context_that_goes_takes_its_registrations_with_it_and_runs_none_of_them)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  rig.watcher().forget_context(rig.wire());
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.watcher().delivered(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, an_event_the_host_can_no_longer_be_asked_about_is_dropped_rather_than_kept)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  rig.rig().host().cl_info(CL_INVALID_EVENT, "", 0u);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_delivered_registration_gives_its_event_back_once_and_after_the_callback)
{ Watching watching;
  forget();
  CHECK_EQ(watching.watcher().watch(&watching.event(), 0x6900u, watching.wire(), CL_COMPLETE,
                                    &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(watching.owner().releases(), 0u);

  const cl_int queued = CL_QUEUED;
  watching.rig().host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&queued), sizeof queued);
  CHECK(watching.thread().step());
  CHECK_EQ(watching.owner().releases(), 0u);

  const cl_int complete = CL_COMPLETE;
  watching.rig().host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete),
                                sizeof complete);
  CHECK(watching.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(watching.owner().releases(), 1u);
  CHECK(watching.owner().last() == &watching.event());
  CHECK_EQ(watching.owner().deliveries(), 1u);
  CHECK(watching.owner().delivered_before_release());
  CHECK(ran.event == reinterpret_cast<cl_event>(&watching.event()));

  CHECK(watching.thread().step());
  CHECK_EQ(watching.owner().releases(), 1u);
  CHECK(watching.watcher().invariants_hold()); }

TEST(cl_event_watcher, an_event_whose_context_is_forgotten_is_not_given_back)
{ Watching watching;
  forget();
  CHECK_EQ(watching.watcher().watch(&watching.event(), 0x6900u, watching.wire(), CL_COMPLETE,
                                    &note, &user_data, false),
           CL_SUCCESS);
  watching.watcher().forget_context(watching.wire());
  CHECK_EQ(watching.watcher().registered(), 0u);
  CHECK_EQ(watching.owner().releases(), 0u);
  CHECK_EQ(ran.times, 0u);
  CHECK(watching.watcher().invariants_hold()); }

namespace {

void nothing_ran(cl_event, cl_int, void *) {}

}

TEST(cl_event_watcher, a_registrations_invariant_catches_a_callback_that_could_not_run)
{ Watching   rig;
  ClObject  &event = rig.event();
  const auto broken = [&rig, &event](void (*corrupt)(ClWatchedEvent &)) {
      ClWatchedEvent watched;
      CHECK(watched.take(&event, 0x6900u, rig.wire(), CL_COMPLETE, nothing_ran, nullptr));
      CHECK(watched.invariants_hold());
      corrupt(watched);
      return watched.invariants_hold();
  };
  CHECK(!broken(ClWatchedEventProbe::forget_event));
  CHECK(!broken(ClWatchedEventProbe::forget_transport));
  CHECK(!broken(ClWatchedEventProbe::forget_notify));
  CHECK(!broken(ClWatchedEventProbe::forget_wire));

  ClWatchedEvent empty;
  CHECK(empty.invariants_hold());
  ClWatchedEventProbe::stray_event(empty, &event);
  CHECK(!empty.invariants_hold());
  ClWatchedEventProbe::stray_event(empty, nullptr);
  ClWatchedEventProbe::stray_transport(empty, rig.wire());
  CHECK(!empty.invariants_hold());
  ClWatchedEventProbe::stray_transport(empty, nullptr);
  ClWatchedEventProbe::stray_notify(empty, nothing_ran);
  CHECK(!empty.invariants_hold());
  ClWatchedEventProbe::stray_notify(empty, nullptr);
  ClWatchedEventProbe::stray_wire(empty);
  CHECK(!empty.invariants_hold()); }

TEST(cl_event_watcher, a_registration_with_no_thread_to_run_it_on_is_given_back)
{ Watching rig;
  rig.thread().refuse_start();
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.rig().log().holding("no thread to watch events on"), 1u);
  CHECK(rig.watcher().invariants_hold());

  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, forgetting_one_context_leaves_every_other_contexts_registrations)
{ Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  rig.rig().host().arena_import(0x78u, 3u);
  av::ObjectStorage<GuestClContext> second;
  CHECK_STATUS(second.construct(rig.rig().connection(), rig.rig().platform(),
                                rig.rig().send_lock(), rig.rig().blocks_lock(), rig.rig().lines(), rig.rig().handles()),
               Status::ok);
  CHECK_STATUS(second.object().make(), Status::ok);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, second.object().wire(), CL_COMPLETE, &note,
                               &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 2u);
  rig.watcher().forget_context(rig.wire());
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.watcher().invariants_hold());
  rig.watcher().forget_context(second.object().wire());
  second.destroy(); }

namespace av::clicd {

struct ClEventWatcherProbe {
    static void miscount(ClEventWatcher &watcher) { watcher.held_ += 1u; }
    static void count_past_the_table(ClEventWatcher &watcher)
    { watcher.held_ = watcher.rows_.size() + 1u; }
    static ClWatchedEvent &registration(ClEventWatcher &watcher, std::uint32_t slot)
    { return watcher.rows_.at(slot).registration_; }
    static void reregister(ClEventWatcher &watcher, std::uint32_t slot, ClObject *event, std::uint64_t wire,
                           av::ClientTransport *transport, ClWatchedEvent::Notify notify)
    {
        if (watcher.rows_.at(slot).registration_.take(event, wire, transport, CL_COMPLETE, notify, nullptr))
            ++watcher.held_;
    }
    static void break_the_row_storage(ClEventWatcher &watcher)
    { av::GrowableArrayProbe::corrupt_capacity(watcher.rows_, 12u); }
    static void break_the_queue_storage(ClEventWatcher &watcher)
    { av::GrowableArrayProbe::corrupt_capacity(watcher.queues_, 12u); }
};

}

using av::clicd::ClEventWatcherProbe;

TEST(cl_event_watcher, the_invariant_catches_a_count_or_a_registration_that_is_wrong)
{ { Watching rig;
    CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note,
                                 &user_data, false),
             CL_SUCCESS);
    CHECK(rig.watcher().invariants_hold());
    ClEventWatcherProbe::miscount(rig.watcher());
    CHECK(!rig.watcher().invariants_hold());
    ClEventWatcherProbe::count_past_the_table(rig.watcher());
    CHECK(!rig.watcher().invariants_hold()); }
  { Watching rig;
    CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note,
                                 &user_data, false),
             CL_SUCCESS);
    ClWatchedEventProbe::forget_notify(ClEventWatcherProbe::registration(rig.watcher(), 0u));
    CHECK(!rig.watcher().invariants_hold()); } }

TEST(cl_event_watcher, a_registration_with_nothing_to_call_runs_nothing)
{ Watching rig;
  ClWatchedEvent watched;
  CHECK(watched.take(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, nothing_ran, nullptr));
  ClWatchedEventProbe::forget_only_the_notify(watched);
  watched.run(CL_COMPLETE);
  CHECK(!watched.invariants_hold()); }

TEST(cl_event_watcher, a_turn_asks_about_a_few_running_events_pauses_and_the_next_goes_on_from_there)
{ forget();
  Watching rig;
  constexpr std::uint32_t asks = ClEventWatcher::asks_per_turn;
  constexpr std::uint32_t done = 4u;
  for (std::uint32_t index = 0u; index < asks + done; ++index)
      CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u + index, rig.wire(), CL_COMPLETE, &note, &user_data, false),
               CL_SUCCESS);
  for (std::uint32_t index = 0u; index < asks; ++index) event_is(rig.rig(), CL_RUNNING);
  for (std::uint32_t index = 0u; index < done; ++index) event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.thread().pauses(), 1u);
  CHECK_EQ(rig.watcher().registered(), asks + done);
  for (std::uint32_t index = 0u; index < asks; ++index) event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, done);
  CHECK_EQ(rig.thread().pauses(), 1u);
  CHECK_EQ(rig.watcher().registered(), asks);
  for (std::uint32_t slot = 0u; slot < asks; ++slot)
      CHECK(ClEventWatcherProbe::registration(rig.watcher(), slot).held());
  for (std::uint32_t slot = asks; slot < asks + done; ++slot)
      CHECK(!ClEventWatcherProbe::registration(rig.watcher(), slot).held());
  CHECK_EQ(rig.thread().rests(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, of_one_queues_events_only_the_oldest_is_asked_about_and_the_next_follows_at_once)
{ forget();
  Watching rig;
  ClObject queue(dispatch, rig.rig().platform(), ClObject::Kind::command_queue, 0x5100u, nullptr, nullptr);
  ClObject first(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6901u, nullptr, &queue);
  ClObject second(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6902u, nullptr, &queue);
  ClObject third(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6903u, nullptr, &queue);
  first.stamp(queue.take_ordinal());
  second.stamp(queue.take_ordinal());
  third.stamp(queue.take_ordinal());
  CHECK_EQ(rig.watcher().watch(&third, 0x6903u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&second, 0x6902u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&first, 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.thread().pauses(), 1u);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK(ran.event == reinterpret_cast<cl_event>(&first));
  CHECK_EQ(rig.thread().pauses(), 1u);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 2u);
  CHECK(ran.event == reinterpret_cast<cl_event>(&second));
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 2u);
  CHECK_EQ(rig.thread().pauses(), 2u);
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_turn_on_driven_contexts_alone_pauses_until_the_driver_wakes_it)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, true),
           CL_SUCCESS);
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().pauses(), 1u);
  CHECK_EQ(rig.thread().driven_pauses(), 1u);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  event_is(rig.rig(), CL_RUNNING);
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().pauses(), 2u);
  CHECK_EQ(rig.thread().driven_pauses(), 1u);
  event_is(rig.rig(), CL_RUNNING);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().pauses(), 3u);
  CHECK_EQ(rig.thread().driven_pauses(), 2u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_turn_whose_asks_ran_out_does_not_sleep_on_the_driver)
{ forget();
  Watching rig;
  constexpr std::uint32_t asks = ClEventWatcher::asks_per_turn;
  for (std::uint32_t index = 0u; index < asks + 1u; ++index)
      CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u + index, rig.wire(), CL_COMPLETE, &note, &user_data, true),
               CL_SUCCESS);
  for (std::uint32_t index = 0u; index < asks; ++index) event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().pauses(), 1u);
  CHECK_EQ(rig.thread().driven_pauses(), 0u);
  event_is(rig.rig(), CL_COMPLETE);
  for (std::uint32_t index = 0u; index < asks; ++index) event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.thread().pauses(), 1u);
  for (std::uint32_t index = 0u; index < asks; ++index) event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().pauses(), 2u);
  CHECK_EQ(rig.thread().driven_pauses(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_wait_elsewhere_has_the_thread_running_and_waits_on_it)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().wake_handle(), 0u);
  rig.thread().offer_wake(0x2A0u);
  CHECK_EQ(rig.watcher().wake_handle(), 0x2A0u);
  CHECK(!rig.thread().running());
  CHECK(rig.watcher().attend());
  CHECK(rig.thread().running());
  CHECK(rig.watcher().attend());
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.watcher().woken(), 0u);
  rig.thread().driver_woke();
  CHECK_EQ(rig.watcher().woken(), 1u);
  rig.watcher().await_woken(1u, 5000u);
  CHECK_EQ(rig.thread().awaits(), 1u);
  CHECK_EQ(rig.thread().last_seen(), 1u);
  CHECK_EQ(rig.thread().last_timeout(), 5000u);
  Watching refused;
  refused.thread().refuse_start();
  CHECK(!refused.watcher().attend()); }

TEST(cl_watched_event, a_status_is_reached_by_what_is_as_far_along_as_the_one_wanted_or_failed)
{ Watching rig;
  ClWatchedEvent watched;
  CHECK_EQ(true, watched.take(&rig.event(), 0x6900u, rig.wire(), CL_RUNNING, &note, &user_data));
  CHECK(!watched.reached_by(CL_QUEUED));
  CHECK(!watched.reached_by(CL_SUBMITTED));
  CHECK(watched.reached_by(CL_RUNNING));
  CHECK(watched.reached_by(CL_COMPLETE));
  CHECK(watched.reached_by(-5));
  watched.give_up();
  CHECK_EQ(true, watched.take(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data));
  CHECK(!watched.reached_by(CL_RUNNING));
  CHECK(watched.reached_by(CL_COMPLETE));
  CHECK(watched.reached_by(-1));
  CHECK(watched.invariants_hold()); }

namespace {

ClEventWatcher *forgetting_watcher = nullptr;
av::ClientTransport *forgotten_wire = nullptr;

void CL_CALLBACK forget_the_context(cl_event event, cl_int status, void *data)
{
    note(event, status, data);
    forgetting_watcher->forget_context(forgotten_wire);
}

}

TEST(cl_event_watcher, a_callback_that_forgets_its_context_leaves_the_later_rows_unrun)
{ forget();
  Watching rig;
  forgetting_watcher = &rig.watcher();
  forgotten_wire = rig.wire();
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &forget_the_context, &user_data, false),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.watcher().delivered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

namespace {

class Interfering {
public:
    enum class Kind { forget, other_event, other_wire };

    Interfering(Watching &rig, Kind kind, ClObject *other) : rig_(rig), kind_(kind), other_(other) {}

    void arm()
    {
        rig_.rig().kmt().play_at_wait(1u, &Interfering::fire, this);
    }

private:
    static void fire(void *self) { static_cast<Interfering *>(self)->interfere(); }

    void interfere()
    {
        rig_.watcher().forget_context(rig_.wire());
        if (kind_ == Kind::other_event)
            ClEventWatcherProbe::reregister(rig_.watcher(), 0u, other_, 0x6900u, rig_.wire(), &note);
        if (kind_ == Kind::other_wire)
            ClEventWatcherProbe::reregister(rig_.watcher(), 0u, &rig_.event(), 0x6A00u, rig_.wire(), &note);
    }

    Watching &rig_;
    Kind      kind_;
    ClObject *other_;
};

void a_registration_changed_under_a_turn_is_not_delivered(Interfering::Kind kind)
{ forget();
  Watching rig;
  ClObject other(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6902u, nullptr, nullptr);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
           CL_SUCCESS);
  Interfering interfering(rig, kind, &other);
  event_is(rig.rig(), CL_COMPLETE);
  interfering.arm();
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.watcher().delivered(), 0u);
  CHECK_EQ(rig.watcher().registered(), kind == Interfering::Kind::forget ? 0u : 1u);
  CHECK(rig.watcher().invariants_hold());
  rig.watcher().forget_context(rig.wire()); }

}

TEST(cl_event_watcher, a_registration_forgotten_while_its_event_was_asked_about_is_not_delivered)
{ a_registration_changed_under_a_turn_is_not_delivered(Interfering::Kind::forget); }

TEST(cl_event_watcher, a_slot_given_to_another_event_while_the_old_one_was_asked_about_is_not_delivered)
{ a_registration_changed_under_a_turn_is_not_delivered(Interfering::Kind::other_event); }

TEST(cl_event_watcher, a_slot_given_to_another_wire_while_the_old_one_was_asked_about_is_not_delivered)
{ a_registration_changed_under_a_turn_is_not_delivered(Interfering::Kind::other_wire); }

namespace {

struct Queued {
    explicit Queued(Watching &rig)
        : queue_a(dispatch, rig.rig().platform(), ClObject::Kind::command_queue, 0x5100u, nullptr, nullptr),
          queue_b(dispatch, rig.rig().platform(), ClObject::Kind::command_queue, 0x5101u, nullptr, nullptr),
          a1(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6911u, nullptr, &queue_a),
          a2(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6912u, nullptr, &queue_a),
          b1(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6921u, nullptr, &queue_b)
    {
        a1.stamp(queue_a.take_ordinal());
        a2.stamp(queue_a.take_ordinal());
        b1.stamp(queue_b.take_ordinal());
    }

    ClObject queue_a, queue_b, a1, a2, b1;
};

}

TEST(cl_event_watcher, an_event_never_stamped_on_its_queue_is_always_asked_about)
{ forget();
  Watching rig;
  ClObject queue(dispatch, rig.rig().platform(), ClObject::Kind::command_queue, 0x5100u, nullptr, nullptr);
  ClObject unstamped(dispatch, rig.rig().platform(), ClObject::Kind::event, 0x6901u, nullptr, &queue);
  CHECK_EQ(rig.watcher().watch(&unstamped, 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK(ran.event == reinterpret_cast<cl_event>(&unstamped));
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, each_queue_has_its_own_oldest_event_and_later_ones_wait_behind_it)
{ forget();
  Watching rig;
  Queued queued(rig);
  CHECK_EQ(rig.watcher().watch(&queued.a1, 0x6911u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&queued.b1, 0x6921u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&queued.a2, 0x6912u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 2u);
  CHECK_EQ(rig.watcher().registered(), 1u);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 3u);
  CHECK(ran.event == reinterpret_cast<cl_event>(&queued.a2));
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_queue_the_watcher_cannot_note_has_all_its_events_asked_about)
{ forget();
  Watching rig;
  Queued queued(rig);
  CHECK_EQ(rig.watcher().watch(&queued.a1, 0x6911u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  CHECK_EQ(rig.watcher().watch(&queued.a2, 0x6912u, rig.wire(), CL_COMPLETE, &note, &user_data, false), CL_SUCCESS);
  event_is(rig.rig(), CL_RUNNING);
  event_is(rig.rig(), CL_COMPLETE);
  rig.rig().platform().fail_allocation_at(1u);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK(ran.event == reinterpret_cast<cl_event>(&queued.a2));
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, the_invariant_catches_storage_that_is_not_a_table)
{ { Watching rig;
    CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
             CL_SUCCESS);
    CHECK(rig.watcher().invariants_hold());
    ClEventWatcherProbe::break_the_row_storage(rig.watcher());
    CHECK(!rig.watcher().invariants_hold()); }
  { Watching rig;
    Queued queued(rig);
    CHECK_EQ(rig.watcher().watch(&queued.a1, 0x6911u, rig.wire(), CL_COMPLETE, &note, &user_data, false),
             CL_SUCCESS);
    event_is(rig.rig(), CL_RUNNING);
    CHECK(rig.thread().step());
    CHECK(rig.watcher().invariants_hold());
    ClEventWatcherProbe::break_the_queue_storage(rig.watcher());
    CHECK(!rig.watcher().invariants_hold()); } }
