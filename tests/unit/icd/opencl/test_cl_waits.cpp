/* What the ICD does instead of asking the host to block.
 *
 * clFinish, clWaitForEvents and a blocking transfer all have to come back only
 * when the work is done, and the one way not to do that is to send the device
 * a command whose host implementation blocks: the device's worker sits inside
 * it, and a guest waiting on the fence past TdrDelay has dxgkrnl reset the
 * adapter. So the wait is here, over status queries the host answers at once,
 * and icd::SlicedWait is what bounds it.
 *
 * The three properties SlicedWait gives this are what a wait is judged by: the
 * first round is always made, so work already done costs one round trip and
 * not none; a deadline that has passed stops the asking; and a counter that
 * cannot say how long a tick is gets one round rather than an endless loop. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/counting_event_owner.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstdint>

using av::ClContextRig;
using av::CountingEventOwner;
using av::MockKmt;
using av::ScriptedThread;
using av::Status;
using av::clicd::ClBlockingWait;
using av::clicd::ClEventWatcher;
using av::clicd::GuestClContext;
using av::icd::SlicedWait;

namespace {

constexpr std::uint64_t host_event = 0x6900u;
constexpr std::uint64_t other_event = 0x6901u;
constexpr std::uint64_t host_queue = 0x6500u;

class Waiting {
public:
    /* `wake`: the process's driver wake, handed to a driver that takes it -
     * so the context's waits are driven - or 0 for none. */
    explicit Waiting(std::uint64_t wake = 0u)
        : rig_(), thread_(), owner_(), watcher_(thread_, rig_.log(), rig_.send_lock(), owner_),
          waits_(rig_.kmt(), watcher_)
    {
        rig_.host().blob_import(0u, 0x77u);
        if (wake != 0u) {
            thread_.offer_wake(wake);
            rig_.kmt().accept_wake_events();
        }
        static_cast<void>(
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.blocks_lock(), rig_.log(), rig_.handles()));
        CHECK_STATUS(context_.object().make(0u, watcher_.wake_handle()), Status::ok);
    }
    ~Waiting() { context_.destroy(); }

    ClContextRig        &rig() { return rig_; }
    ClBlockingWait      &waits() { return waits_; }
    ScriptedThread      &thread() { return thread_; }
    GuestClContext      &context() { return context_.object(); }
    bool                 driven() { return context_.object().wakes(); }
    /* Every read of the clock a millisecond on, so a wait's first round is
     * already past the millisecond it asks back to back in. */
    void                 past_the_spin() { rig_.kmt().reads_clock(nullptr, MockKmt::ticks_per_second / 1000u); }
    av::ClientTransport *wire() { return context_.object().wire(); }

    /* The host's answer to one clGetEventInfo. */
    void answers(cl_int status)
    {
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&status), sizeof status);
    }

private:
    ClContextRig                      rig_;
    ScriptedThread                    thread_;
    CountingEventOwner                owner_;
    ClEventWatcher                    watcher_;
    ClBlockingWait                    waits_;
    av::ObjectStorage<GuestClContext> context_;
};

}  // namespace

TEST(cl_waits, work_already_done_costs_one_round_and_not_none)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  const std::uint32_t commands = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_SUCCESS);
  /* Asked once, which is what makes a zero timeout a poll rather than a
   * refusal. */
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u);
  CHECK_EQ(waiting.rig().kmt().commands(), commands + 1u); }

TEST(cl_waits, a_zero_timeout_still_asks_once)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 0u, false), CL_SUCCESS);
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u); }

TEST(cl_waits, a_deadline_that_passes_ends_the_wait_rather_than_asking_for_ever)
{ Waiting waiting;
  /* Every round trip costs a tenth of a second on this clock, so a wait of
   * one second gives up after a bounded number of rounds - and the work is
   * never done, so only the deadline can end it. */
  waiting.rig().kmt().spend_on_submit(MockKmt::ticks_per_second / 10u);
  for (std::uint32_t index = 0u; index < 32u; ++index) waiting.answers(CL_RUNNING);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u,
                                          1000000000u /* one second */, false),
           CL_SUCCESS);
  const std::uint64_t asked = waiting.waits().rounds() - rounds;
  CHECK(asked > 1u);
  CHECK(asked <= 12u); }

TEST(cl_waits, a_clock_that_cannot_say_how_long_a_tick_is_gets_one_round)
{ Waiting waiting;
  waiting.rig().kmt().set_frequency(0u);
  waiting.answers(CL_RUNNING);
  const std::uint64_t rounds = waiting.waits().rounds();
  /* A finite wait on a counter that measures nothing cannot know whether the
   * deadline passed, so it asks once and answers - never loops. */
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 1000000000u, false),
           CL_SUCCESS);
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u); }

TEST(cl_waits, every_event_is_asked_about_and_the_wait_ends_when_the_last_one_is_done)
{ Waiting waiting;
  const std::uint64_t both[] = {host_event, other_event};
  /* First round: the first is done, the second is not, so the round ends
   * there. Second round: both. */
  waiting.answers(CL_COMPLETE);
  waiting.answers(CL_RUNNING);
  waiting.answers(CL_COMPLETE);
  waiting.answers(CL_COMPLETE);
  const std::uint32_t commands = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), both, 2u, SlicedWait::forever, false),
           CL_SUCCESS);
  CHECK_EQ(waiting.rig().kmt().commands(), commands + 4u); }

TEST(cl_waits, an_event_that_failed_ends_the_wait_with_the_error_the_api_names)
{ Waiting waiting;
  waiting.answers(-5 /* CL_OUT_OF_RESOURCES, as a command's own status */);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST); }

TEST(cl_waits, a_host_that_will_not_answer_the_query_ends_the_wait_with_its_own_error)
{ Waiting waiting;
  waiting.rig().host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_INVALID_EVENT); }

TEST(cl_waits, a_wait_for_nothing_is_the_callers_mistake_and_asks_the_host_nothing)
{ Waiting waiting;
  const std::uint32_t commands = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), nullptr, 1u, SlicedWait::forever, false),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 0u, SlicedWait::forever, false),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.rig().kmt().commands(), commands);
  CHECK_EQ(waiting.waits().rounds(), 0u); }

/* A wait this side invented, rather than one the application asked for.
 *
 * Every blocking transfer and clFinish is turned into an enqueue and a wait on
 * the event it produced. OpenCL never promises a queued command starts before
 * the queue is flushed, and the application handed the ICD a blocking call and
 * never saw a queue to flush - so the flush is this side's to send, and a wait
 * without it sits for ever on a host driver that starts work only at one. */

TEST(cl_waits, a_wait_on_its_own_enqueue_flushes_the_queue_before_it_asks)
{ Waiting waiting;
  waiting.rig().host().cl_result(CL_SUCCESS); /* the flush */
  waiting.answers(CL_COMPLETE);
  const std::uint32_t first = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event, false),
           CL_SUCCESS);
  /* The flush goes first. Asking the host how an event is doing before the
   * queue has been told to start is what never comes back. */
  CHECK_EQ(waiting.rig().kmt().commands(), first + 2u);
  CHECK_EQ(waiting.rig().kmt().command_id(first), av::clproto::kCmd_clFlush);
  CHECK_EQ(waiting.rig().kmt().command_id(first + 1u), av::clproto::kCmd_clGetEventInfo); }

TEST(cl_waits, a_flush_the_host_refuses_ends_the_wait_instead_of_asking_for_ever)
{ Waiting waiting;
  /* A queue that will not flush is one whose work may never start, so the
   * wait that would follow could not end. The caller is told instead. */
  waiting.rig().host().cl_result(CL_INVALID_COMMAND_QUEUE);
  const std::uint32_t first = waiting.rig().kmt().commands();
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event, false),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(waiting.rig().kmt().commands(), first + 1u);
  CHECK_EQ(waiting.waits().rounds(), rounds); }

TEST(cl_waits, a_wait_on_no_queue_or_no_event_is_the_callers_mistake_and_sends_nothing)
{ Waiting waiting;
  const std::uint32_t first = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), 0u, host_event, false),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, 0u, false),
           CL_INVALID_VALUE);
  /* Not even the flush: neither names a queue this could flush. */
  CHECK_EQ(waiting.rig().kmt().commands(), first);
  CHECK_EQ(waiting.waits().rounds(), 0u); }

/* ---- driven by the driver's wake -------------------------------------------
 *
 * A context the driver wakes (GuestClContext::wakes) has a wait sleep between
 * rounds instead of asking on a timer: it has the host raise when the first
 * event still outstanding finishes - the host's own callback on it - and
 * sleeps until the event watcher's thread sees the driver's wake. */

namespace {

constexpr std::uint64_t wake = 0x2A0u;

/* What happens on other threads while a driven wait sleeps, one entry per
 * sleep: the host finishes something, raises, and the watcher's thread sees
 * the driver's wake - then the host's answers for the next round. */
struct Sleeps {
    Waiting      *waiting;
    std::uint32_t slept = 0u;
};

void the_event_finishes(void *context)
{
    auto *const sleeps = static_cast<Sleeps *>(context);
    sleeps->waiting->thread().driver_woke();
    sleeps->waiting->answers(CL_COMPLETE);
    ++sleeps->slept;
}

void two_events_finish_one_by_one(void *context)
{
    auto *const sleeps = static_cast<Sleeps *>(context);
    Waiting    &waiting = *sleeps->waiting;
    waiting.thread().driver_woke();
    switch (sleeps->slept++) {
    case 0u:   /* the first: the second is asked to raise */
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_QUEUED);
        waiting.rig().host().cl_result(CL_SUCCESS);
        break;
    case 1u:   /* a wake for something else: the second is not asked again */
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_RUNNING);
        break;
    default:
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_COMPLETE);
        break;
    }
}

}  // namespace

TEST(cl_waits, a_context_hands_the_driver_the_process_s_wake_and_its_waits_are_driven)
{ Waiting waiting(wake);
  CHECK(waiting.driven());
  CHECK_EQ(waiting.rig().kmt().wake_events(), 1u);
  CHECK_EQ(waiting.rig().kmt().wake_event(), wake);
  CHECK(waiting.rig().log().holding("avcl: the driver wakes this context's waits") == 1u);
  /* A driver that will not - one from before the record, or on a device that
   * never raises - leaves the context asking on a timer. */
  ClContextRig rig;
  ScriptedThread thread;
  thread.offer_wake(wake);
  rig.host().blob_import(0u, 0x77u);
  av::ObjectStorage<GuestClContext> refused;
  static_cast<void>(refused.construct(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(),
                                      rig.handles()));
  CHECK_STATUS(refused.object().make(0u, thread.wake_handle()), Status::ok);
  CHECK(!refused.object().wakes());
  CHECK(rig.log().holding("avcl: this context's waits ask on a timer; the driver does not wake them") == 1u);
  refused.destroy();
  /* And a process with no wake hands nothing over. */
  Waiting plain;
  CHECK(!plain.driven());
  CHECK_EQ(plain.rig().kmt().wake_events(), 0u); }

TEST(cl_waits, a_driven_wait_has_the_host_raise_and_asks_nothing_until_the_driver_wakes_it)
{ Waiting waiting(wake);
  waiting.past_the_spin();
  Sleeps sleeps{&waiting};
  waiting.thread().on_await(&the_event_finishes, &sleeps);
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_SUCCESS);   /* the raise */
  const std::uint32_t first = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, true),
           CL_SUCCESS);
  /* A round, the raise, one sleep, a round: nothing asked while the host worked. */
  CHECK_EQ(waiting.rig().kmt().commands(), first + 3u);
  CHECK_EQ(waiting.rig().kmt().command_id(first), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(waiting.rig().kmt().command_id(first + 1u), av::clproto::kCmd_clSetEventCallback);
  CHECK_EQ(waiting.rig().kmt().command_id(first + 2u), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(sleeps.slept, 1u);
  CHECK_EQ(waiting.waits().raises_asked(), 1u);
  CHECK_EQ(waiting.waits().rounds(), 2u);
  /* The thread the driver's wake ends is running, and the sleep waited past
   * what had been seen before the round, a slice at most. */
  CHECK_EQ(waiting.thread().starts(), 1u);
  CHECK_EQ(waiting.thread().last_seen(), 0u);
  CHECK(waiting.thread().last_timeout() != 0u);
  CHECK(waiting.thread().last_timeout() <= SlicedWait::slice_ns); }

TEST(cl_waits, a_driven_wait_has_the_host_raise_on_each_event_once_as_it_comes_first)
{ Waiting waiting(wake);
  waiting.past_the_spin();
  Sleeps sleeps{&waiting};
  waiting.thread().on_await(&two_events_finish_one_by_one, &sleeps);
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_SUCCESS);
  const std::uint64_t both[] = {host_event, other_event};
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), both, 2u, SlicedWait::forever, true), CL_SUCCESS);
  CHECK_EQ(sleeps.slept, 3u);
  CHECK_EQ(waiting.waits().raises_asked(), 2u);
  CHECK_EQ(waiting.waits().rounds(), 4u);
  CHECK_EQ(waiting.rig().kmt().queued_answers(), 0u); }

TEST(cl_waits, a_host_that_will_not_raise_leaves_the_wait_asking_on_a_timer)
{ Waiting waiting(wake);
  waiting.past_the_spin();
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_INVALID_OPERATION);
  waiting.answers(CL_COMPLETE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, true),
           CL_SUCCESS);
  CHECK_EQ(waiting.thread().awaits(), 0u);
  CHECK_EQ(waiting.waits().raises_asked(), 1u);
  CHECK_EQ(waiting.waits().rounds(), 2u); }

TEST(cl_waits, a_driven_wait_with_no_thread_to_tell_it_asks_on_a_timer)
{ Waiting waiting(wake);
  waiting.thread().refuse_start();
  waiting.answers(CL_RUNNING);
  waiting.answers(CL_COMPLETE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, true),
           CL_SUCCESS);
  CHECK_EQ(waiting.thread().awaits(), 0u);
  CHECK_EQ(waiting.waits().raises_asked(), 0u);
  CHECK_EQ(waiting.waits().rounds(), 2u); }
