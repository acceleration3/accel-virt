/* The callbacks an application registers and then waits for without asking
 * again.
 *
 * Three rules make clSetEventCallback safe, and each of them is a case here. A
 * callback runs **once**. It runs with **no ICD lock held**, because it may
 * call back into the ICD - and an application's callback that releases its own
 * event is the ordinary case, not a hostile one. And the registration is
 * forgotten **before** it runs, so what the callback releases is something
 * this is no longer holding.
 *
 * The thread is the scripted one: a test says when a turn happens, which is
 * what makes "once, after this, with this status" something a test can state
 * exactly rather than wait for.
 *
 * One registration is a `ClWatchedEvent`, which the watcher copies and gives
 * up before running - so `reached_by` and `run` are what a completed event
 * amounts to. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/counting_event_owner.hpp"
#include "harness/icd/counting_event_owner.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_object.hpp"
#include "icd/opencl/cl_watched_event.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <cstdint>

using av::ClContextRig;
using av::CountingEventOwner;
using av::CountingEventOwner;
using av::ScriptedThread;
using av::Status;
using av::clicd::ClEventWatcher;
using av::clicd::ClWatchedEvent;
using av::clicd::ClWatchedEventProbe;
using av::clicd::ClObject;
using av::clicd::GuestClContext;

namespace av::clicd {

/* A registration is either wholly live or wholly empty. Halfway is a
 * callback that could not be run, or a slot believed free that a completion
 * would still fire. */
struct ClWatchedEventProbe {
    static void forget_event(ClWatchedEvent &watched) { watched.event_ = nullptr; }
    /* A registration with nothing to call. Running one is what a watcher
     * would do to a row it believed was armed. */
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

}  // namespace av::clicd

namespace {

/* What a callback did, so a test can say it happened once and with what. */
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

/* An event object, as the table would hold one. Only its address is used. */
cl_icd_dispatch dispatch = {};

class Watching {
public:
    Watching()
        : rig_(), thread_(), owner_(), watcher_(thread_, rig_.log(), owner_),
          event_(dispatch, ClObject::Kind::event, 0x6900u, nullptr, nullptr)
    {
        rig_.host().blob_import(0u, 0x77u);
        static_cast<void>(
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.log(), rig_.handles()));
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

/* What the host answers clGetEventInfo: one execution status. */
void event_is(ClContextRig &rig, cl_int status)
{
    rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&status), sizeof status);
}

}  // namespace

TEST(cl_event_watcher, a_watcher_with_nothing_registered_runs_no_thread)
{ Watching rig;
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(!rig.thread().running());
  CHECK_EQ(rig.thread().starts(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, registering_the_first_callback_starts_the_thread)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.thread().running());
  CHECK_EQ(rig.thread().starts(), 1u);
  /* A second registration joins the thread that is already running. */
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.watcher().registered(), 2u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_callback_runs_once_when_its_event_completes)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);

  /* The host says the event has not finished; nothing runs, and the watcher
   * asks to be called again. */
  event_is(rig.rig(), CL_RUNNING);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.watcher().registered(), 1u);

  /* Now it has. */
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, CL_COMPLETE);
  CHECK(ran.user_data == &user_data);
  CHECK(ran.event == reinterpret_cast<cl_event>(&rig.event()));
  /* Once: the registration is gone, so a later turn cannot run it again. */
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.watcher().delivered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

/* With nothing left to watch the loop rests rather than ends: a thread that
 * ended when its last callback ran was one the next registration found still
 * "running", so it never started again and no callback after the first batch
 * ever came - which is what kept Affinity from making a document. A later
 * registration wakes the rest, on the thread that never went away. */
TEST(cl_event_watcher, the_loop_rests_when_nothing_is_left_and_a_registration_wakes_it)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.thread().rests(), 0u);
  /* Nothing registered: a rest, not an end. */
  CHECK(rig.thread().step());
  CHECK_EQ(rig.thread().rests(), 1u);
  CHECK(rig.thread().running());
  /* The second registration wakes it, and its callback comes. */
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, rig.wire(), CL_COMPLETE, &note, &user_data),
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
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  /* A negative status is a command that failed, and the API says a callback
   * runs then as well - an application waiting on a completion it will never
   * get is worse than one told it failed. */
  event_is(rig.rig(), -5);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, -5); }

TEST(cl_event_watcher, a_callback_registered_for_an_earlier_status_runs_at_that_status)
{ forget();
  Watching rig;
  /* OpenCL counts down - CL_QUEUED is 3 and CL_COMPLETE is 0 - so a
   * registration's status_wanted is a floor rather than a ceiling. */
  /* CL_RUNNING is earlier than CL_COMPLETE in the API's ordering, and a
   * callback registered for it runs when the command starts. */
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_RUNNING, &note, &user_data),
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
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(nullptr, 0x6900u, rig.wire(), CL_COMPLETE, &note, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0u, rig.wire(), CL_COMPLETE, &note, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, nullptr, CL_COMPLETE, &note, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(!rig.thread().running());
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_watcher_that_is_full_refuses_and_keeps_what_it_has)
{ Watching rig;
  for (std::uint32_t index = 0u; index < ClEventWatcher::max_registrations; ++index) {
      CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u + index, rig.wire(), CL_COMPLETE, &note,
                                   nullptr),
               CL_SUCCESS);
  }
  CHECK_EQ(rig.watcher().registered(), ClEventWatcher::max_registrations);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x7000u, rig.wire(), CL_COMPLETE, &note, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.watcher().registered(), ClEventWatcher::max_registrations);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_context_that_goes_takes_its_registrations_with_it_and_runs_none_of_them)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  /* Releasing a context is not the event completing: a callback that has not
   * fired does not fire because its world went away. */
  rig.watcher().forget_context(rig.wire());
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(ran.times, 0u);
  CHECK_EQ(rig.watcher().delivered(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, an_event_the_host_can_no_longer_be_asked_about_is_dropped_rather_than_kept)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  /* The host refuses the query - a lost device, a released event. Keeping the
   * registration would leave the application waiting for ever, so the
   * callback runs with what it was told. */
  rig.rig().host().cl_info(CL_INVALID_EVENT, "", 0u);
  CHECK(rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, a_delivered_registration_gives_its_event_back_once_and_after_the_callback)
{ Watching watching;
  forget();
  CHECK_EQ(watching.watcher().watch(&watching.event(), 0x6900u, watching.wire(), CL_COMPLETE,
                                    &note, &user_data),
           CL_SUCCESS);
  /* Nothing is given back while it is still registered. */
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
  /* Once, and it is the event the callback was handed - which it could only
   * have been handed because the reference was still held while it ran. */
  CHECK_EQ(watching.owner().releases(), 1u);
  CHECK(watching.owner().last() == &watching.event());
  /* And whatever the event owed the application arrived first: the watcher
   * asks the owner to deliver before it runs the callback, because a
   * callback on a non-blocking read has to see the bytes. */
  CHECK_EQ(watching.owner().deliveries(), 1u);
  CHECK(watching.owner().delivered_before_release());
  CHECK(ran.event == reinterpret_cast<cl_event>(&watching.event()));

  /* And a further turn delivers nothing, so nothing is given back twice. */
  CHECK(watching.thread().step());
  CHECK_EQ(watching.owner().releases(), 1u);
  CHECK(watching.watcher().invariants_hold()); }

TEST(cl_event_watcher, an_event_whose_context_is_forgotten_is_not_given_back)
{ Watching watching;
  forget();
  CHECK_EQ(watching.watcher().watch(&watching.event(), 0x6900u, watching.wire(), CL_COMPLETE,
                                    &note, &user_data),
           CL_SUCCESS);
  /* The context is going, and its objects with it: there is nothing left to
   * give the reference back to, and giving it back would reach into a table
   * that no longer holds the event. */
  watching.watcher().forget_context(watching.wire());
  CHECK_EQ(watching.watcher().registered(), 0u);
  CHECK_EQ(watching.owner().releases(), 0u);
  CHECK_EQ(ran.times, 0u);
  CHECK(watching.watcher().invariants_hold()); }

/* The invariant of one registration: live means every one of the four things
 * a completion needs is there, and empty means none of them is. */
namespace {

void nothing_ran(cl_event, cl_int, void *) {}

}  // namespace

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

  /* And the empty half: a slot believed free that a completion would still
   * find something in. */
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

/* A watcher with no thread to run callbacks on.
 *
 * The thread starts on the first registration and not before, so a process
 * that never asks for a callback never has one. When the OS refuses it, the
 * registration has to go back: a callback registered on a watcher that will
 * never turn is one the application waits for forever. */
TEST(cl_event_watcher, a_registration_with_no_thread_to_run_it_on_is_given_back)
{ Watching rig;
  rig.thread().refuse_start();
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.rig().log().holding("no thread to watch events on"), 1u);
  CHECK(rig.watcher().invariants_hold());

  /* And the next registration still works, because nothing was kept. */
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

/* Forgetting a context drops that context's registrations and nobody else's:
 * they share a watcher, and a callback on another context's event must still
 * run after this one dies. */
TEST(cl_event_watcher, forgetting_one_context_leaves_every_other_contexts_registrations)
{ Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  /* A second context, sharing the watcher: its registration must survive the
   * first context dying. */
  rig.rig().host().blob_import(0u, 0x78u);
  av::ObjectStorage<GuestClContext> second;
  CHECK_STATUS(second.construct(rig.rig().connection(), rig.rig().platform(),
                                rig.rig().send_lock(), rig.rig().log(), rig.rig().handles()),
               Status::ok);
  CHECK_STATUS(second.object().make(), Status::ok);
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6901u, second.object().wire(), CL_COMPLETE, &note,
                               &user_data),
           CL_SUCCESS);
  CHECK_EQ(rig.watcher().registered(), 2u);
  rig.watcher().forget_context(rig.wire());
  /* One went, one stayed. */
  CHECK_EQ(rig.watcher().registered(), 1u);
  CHECK(rig.watcher().invariants_hold());
  rig.watcher().forget_context(second.object().wire());
  second.destroy(); }

namespace av::clicd {

struct ClEventWatcherProbe {
    static void miscount(ClEventWatcher &watcher) { watcher.held_ += 1u; }
    static void count_past_the_table(ClEventWatcher &watcher)
    { watcher.held_ = ClEventWatcher::max_registrations + 1u; }
    static ClWatchedEvent &registration(ClEventWatcher &watcher, std::uint32_t slot)
    { return watcher.registrations_[slot]; }
};

}  // namespace av::clicd

using av::clicd::ClEventWatcherProbe;

/* The watcher's own invariant: what it counts and what it holds are one
 * number, and every registration in it is sound. */
TEST(cl_event_watcher, the_invariant_catches_a_count_or_a_registration_that_is_wrong)
{ { Watching rig;
    CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note,
                                 &user_data),
             CL_SUCCESS);
    CHECK(rig.watcher().invariants_hold());
    ClEventWatcherProbe::miscount(rig.watcher());
    CHECK(!rig.watcher().invariants_hold());
    ClEventWatcherProbe::count_past_the_table(rig.watcher());
    CHECK(!rig.watcher().invariants_hold()); }
  { Watching rig;
    CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note,
                                 &user_data),
             CL_SUCCESS);
    /* One registration broken: the watcher says so through it rather than
     * only checking its own count. */
    ClWatchedEventProbe::forget_notify(ClEventWatcherProbe::registration(rig.watcher(), 0u));
    CHECK(!rig.watcher().invariants_hold()); } }

/* A registration with nothing to call runs nothing rather than jumping
 * through a null pointer. The watcher never builds one - take() refuses a
 * null notify - but run() is reached from the watcher's own thread, where a
 * jump into nothing would be a crash with no diagnostic at all. */
TEST(cl_event_watcher, a_registration_with_nothing_to_call_runs_nothing)
{ Watching rig;
  ClWatchedEvent watched;
  CHECK(watched.take(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, nothing_ran, nullptr));
  ClWatchedEventProbe::forget_only_the_notify(watched);
  /* Nothing happens, and nothing crashes. */
  watched.run(CL_COMPLETE);
  CHECK(!watched.invariants_hold()); }
