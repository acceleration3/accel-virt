#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/counting_event_owner.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "proto/submission/command_header.hpp"
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

std::uint32_t sent(const MockKmt &kmt)
{
    std::uint32_t counted = 0u;
    for (std::uint32_t index = 0u; index < kmt.commands(); ++index)
        if (kmt.command_id(index) != av::CommandHeader::channel_stats_id) ++counted;
    return counted;
}

std::uint32_t sent_id(const MockKmt &kmt, std::uint32_t nth)
{
    std::uint32_t counted = 0u;
    for (std::uint32_t index = 0u; index < kmt.commands(); ++index) {
        if (kmt.command_id(index) == av::CommandHeader::channel_stats_id) continue;
        if (counted++ == nth) return kmt.command_id(index);
    }
    return 0u;
}

class Waiting {
public:
    explicit Waiting(std::uint64_t wake = 0u)
        : rig_(), thread_(), owner_(), watcher_(rig_.platform(), thread_, rig_.lines(), rig_.send_lock(), owner_),
          waits_(rig_.kmt(), watcher_)
    {
        rig_.host().arena_import(0x77u, 3u);
        if (wake != 0u) {
            thread_.offer_wake(wake);
            rig_.kmt().accept_wake_events();
        }
        static_cast<void>(
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.blocks_lock(), rig_.lines(), rig_.handles()));
        CHECK_STATUS(context_.object().make(0u, watcher_.wake_handle()), Status::ok);
    }
    ~Waiting() { context_.destroy(); }

    ClContextRig        &rig() { return rig_; }
    ClBlockingWait      &waits() { return waits_; }
    ScriptedThread      &thread() { return thread_; }
    GuestClContext      &context() { return context_.object(); }
    bool                 driven() { return context_.object().wakes(); }
    void                 past_the_spin() { rig_.kmt().reads_clock(nullptr, MockKmt::ticks_per_second / 1000u); }
    av::ClientTransport *wire() { return context_.object().wire(); }

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

}

TEST(cl_waits, work_already_done_costs_one_round_and_not_none)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  const std::uint32_t commands = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_SUCCESS);
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u);
  CHECK_EQ(sent(waiting.rig().kmt()), commands + 1u); }

TEST(cl_waits, a_zero_timeout_still_asks_once)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 0u, false), CL_SUCCESS);
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u); }

TEST(cl_waits, a_deadline_that_passes_ends_the_wait_rather_than_asking_for_ever)
{ Waiting waiting;
  waiting.rig().kmt().spend_on_submit(MockKmt::ticks_per_second / 10u);
  for (std::uint32_t index = 0u; index < 32u; ++index) waiting.answers(CL_RUNNING);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u,
                                          1000000000u , false),
           CL_SUCCESS);
  const std::uint64_t asked = waiting.waits().rounds() - rounds;
  CHECK(asked > 1u);
  CHECK(asked <= 12u); }

TEST(cl_waits, a_clock_that_cannot_say_how_long_a_tick_is_gets_one_round)
{ Waiting waiting;
  waiting.rig().kmt().set_frequency(0u);
  waiting.answers(CL_RUNNING);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 1000000000u, false),
           CL_SUCCESS);
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u); }

TEST(cl_waits, every_event_is_asked_about_and_the_wait_ends_when_the_last_one_is_done)
{ Waiting waiting;
  const std::uint64_t both[] = {host_event, other_event};
  waiting.answers(CL_COMPLETE);
  waiting.answers(CL_RUNNING);
  waiting.answers(CL_COMPLETE);
  waiting.answers(CL_COMPLETE);
  const std::uint32_t commands = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), both, 2u, SlicedWait::forever, false),
           CL_SUCCESS);
  CHECK_EQ(sent(waiting.rig().kmt()), commands + 4u); }

TEST(cl_waits, an_event_that_failed_ends_the_wait_with_the_error_the_api_names)
{ Waiting waiting;
  waiting.answers(-5 );
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST); }

TEST(cl_waits, a_host_that_will_not_answer_the_query_ends_the_wait_with_its_own_error)
{ Waiting waiting;
  waiting.rig().host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, false),
           CL_INVALID_EVENT); }

TEST(cl_waits, a_wait_for_nothing_is_the_callers_mistake_and_asks_the_host_nothing)
{ Waiting waiting;
  const std::uint32_t commands = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), nullptr, 1u, SlicedWait::forever, false),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 0u, SlicedWait::forever, false),
           CL_INVALID_VALUE);
  CHECK_EQ(sent(waiting.rig().kmt()), commands);
  CHECK_EQ(waiting.waits().rounds(), 0u); }

TEST(cl_waits, a_wait_on_its_own_enqueue_flushes_the_queue_before_it_asks)
{ Waiting waiting;
  waiting.rig().host().cl_result(CL_SUCCESS);
  waiting.answers(CL_COMPLETE);
  const std::uint32_t first = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event, false),
           CL_SUCCESS);
  CHECK_EQ(sent(waiting.rig().kmt()), first + 2u);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first), av::clproto::kCmd_clFlush);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first + 1u), av::clproto::kCmd_clGetEventInfo); }

TEST(cl_waits, a_flush_the_host_refuses_ends_the_wait_instead_of_asking_for_ever)
{ Waiting waiting;
  waiting.rig().host().cl_result(CL_INVALID_COMMAND_QUEUE);
  const std::uint32_t first = sent(waiting.rig().kmt());
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event, false),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(sent(waiting.rig().kmt()), first + 1u);
  CHECK_EQ(waiting.waits().rounds(), rounds); }

TEST(cl_waits, a_wait_on_no_queue_or_no_event_is_the_callers_mistake_and_sends_nothing)
{ Waiting waiting;
  const std::uint32_t first = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), 0u, host_event, false),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, 0u, false),
           CL_INVALID_VALUE);
  CHECK_EQ(sent(waiting.rig().kmt()), first);
  CHECK_EQ(waiting.waits().rounds(), 0u); }

namespace {

constexpr std::uint64_t wake = 0x2A0u;

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
    case 0u:
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_QUEUED);
        waiting.rig().host().cl_result(CL_SUCCESS);
        break;
    case 1u:
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_RUNNING);
        break;
    default:
        waiting.answers(CL_COMPLETE);
        waiting.answers(CL_COMPLETE);
        break;
    }
}

}

TEST(cl_waits, a_context_hands_the_driver_the_process_s_wake_and_its_waits_are_driven)
{ Waiting waiting(wake);
  CHECK(waiting.driven());
  CHECK_EQ(waiting.rig().kmt().wake_events(), 1u);
  CHECK_EQ(waiting.rig().kmt().wake_event(), wake);
  CHECK(waiting.rig().log().holding("avcl: the driver wakes this context's waits") == 1u);
  ClContextRig rig;
  ScriptedThread thread;
  thread.offer_wake(wake);
  rig.host().arena_import(0x77u, 3u);
  av::ObjectStorage<GuestClContext> refused;
  static_cast<void>(refused.construct(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.lines(),
                                      rig.handles()));
  CHECK_STATUS(refused.object().make(0u, thread.wake_handle()), Status::ok);
  CHECK(!refused.object().wakes());
  CHECK(rig.log().holding("avcl: this context's waits ask on a timer; the driver does not wake them") == 1u);
  refused.destroy();
  Waiting plain;
  CHECK(!plain.driven());
  CHECK_EQ(plain.rig().kmt().wake_events(), 0u); }

TEST(cl_waits, a_driven_wait_has_the_host_raise_and_asks_nothing_until_the_driver_wakes_it)
{ Waiting waiting(wake);
  waiting.past_the_spin();
  Sleeps sleeps{&waiting};
  waiting.thread().on_await(&the_event_finishes, &sleeps);
  waiting.answers(CL_RUNNING);
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_SUCCESS);
  const std::uint32_t first = sent(waiting.rig().kmt());
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, true),
           CL_SUCCESS);
  CHECK_EQ(sent(waiting.rig().kmt()), first + 4u);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first + 1u), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first + 2u), av::clproto::kCmd_clSetEventCallback);
  CHECK_EQ(sent_id(waiting.rig().kmt(), first + 3u), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(sleeps.slept, 1u);
  CHECK_EQ(waiting.waits().raises_asked(), 1u);
  CHECK_EQ(waiting.waits().rounds(), 3u);
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
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_SUCCESS);
  const std::uint64_t both[] = {host_event, other_event};
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), both, 2u, SlicedWait::forever, true), CL_SUCCESS);
  CHECK_EQ(sleeps.slept, 3u);
  CHECK_EQ(waiting.waits().raises_asked(), 2u);
  CHECK_EQ(waiting.waits().rounds(), 5u);
  CHECK_EQ(waiting.rig().kmt().queued_answers(), 0u); }

TEST(cl_waits, a_host_that_will_not_raise_leaves_the_wait_asking_on_a_timer)
{ Waiting waiting(wake);
  waiting.past_the_spin();
  waiting.answers(CL_RUNNING);
  waiting.answers(CL_RUNNING);
  waiting.rig().host().cl_result(CL_INVALID_OPERATION);
  waiting.answers(CL_COMPLETE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever, true),
           CL_SUCCESS);
  CHECK_EQ(waiting.thread().awaits(), 0u);
  CHECK_EQ(waiting.waits().raises_asked(), 1u);
  CHECK_EQ(waiting.waits().rounds(), 3u); }

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
