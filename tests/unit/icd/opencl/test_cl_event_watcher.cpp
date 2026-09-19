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
#include "icd/opencl/guest_cl_context.hpp"

#include <cstdint>

using av::ClContextRig;
using av::CountingEventOwner;
using av::CountingEventOwner;
using av::ScriptedThread;
using av::Status;
using av::clicd::ClEventWatcher;
using av::clicd::ClObject;
using av::clicd::GuestClContext;

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
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.log()));
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
  CHECK(!rig.thread().step());
  CHECK_EQ(ran.times, 1u);
  CHECK_EQ(ran.status, CL_COMPLETE);
  CHECK(ran.user_data == &user_data);
  CHECK(ran.event == reinterpret_cast<cl_event>(&rig.event()));
  /* Once: the registration is gone, so a later turn cannot run it again. */
  CHECK_EQ(rig.watcher().registered(), 0u);
  CHECK_EQ(rig.watcher().delivered(), 1u);
  CHECK(rig.watcher().invariants_hold()); }

TEST(cl_event_watcher, the_loop_ends_when_nothing_is_left_to_watch)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  event_is(rig.rig(), CL_COMPLETE);
  /* The turn answers false, which is what stops the thread: a watcher with
   * nothing registered has nothing to wait for. */
  CHECK(!rig.thread().step());
  CHECK(!rig.thread().running()); }

TEST(cl_event_watcher, an_event_that_ended_in_an_error_runs_its_callback_too)
{ forget();
  Watching rig;
  CHECK_EQ(rig.watcher().watch(&rig.event(), 0x6900u, rig.wire(), CL_COMPLETE, &note, &user_data),
           CL_SUCCESS);
  /* A negative status is a command that failed, and the API says a callback
   * runs then as well - an application waiting on a completion it will never
   * get is worse than one told it failed. */
  event_is(rig.rig(), -5);
  CHECK(!rig.thread().step());
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
  CHECK(!rig.thread().step());
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
  CHECK(!rig.thread().step());
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
  CHECK(!watching.thread().step());
  CHECK_EQ(ran.times, 1u);
  /* Once, and it is the event the callback was handed - which it could only
   * have been handed because the reference was still held while it ran. */
  CHECK_EQ(watching.owner().releases(), 1u);
  CHECK(watching.owner().last() == &watching.event());
  CHECK(ran.event == reinterpret_cast<cl_event>(&watching.event()));

  /* And a further turn delivers nothing, so nothing is given back twice. */
  CHECK(!watching.thread().step());
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
