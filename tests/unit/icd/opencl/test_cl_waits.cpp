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
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstdint>

using av::ClContextRig;
using av::MockKmt;
using av::Status;
using av::clicd::ClBlockingWait;
using av::clicd::GuestClContext;
using av::icd::SlicedWait;

namespace {

constexpr std::uint64_t host_event = 0x6900u;
constexpr std::uint64_t other_event = 0x6901u;
constexpr std::uint64_t host_queue = 0x6500u;

class Waiting {
public:
    Waiting() : rig_(), waits_(rig_.kmt())
    {
        rig_.host().blob_import(0u, 0x77u);
        static_cast<void>(
            context_.construct(rig_.connection(), rig_.platform(), rig_.send_lock(), rig_.blocks_lock(), rig_.log(), rig_.handles()));
        CHECK_STATUS(context_.object().make(), Status::ok);
    }
    ~Waiting() { context_.destroy(); }

    ClContextRig        &rig() { return rig_; }
    ClBlockingWait      &waits() { return waits_; }
    av::ClientTransport *wire() { return context_.object().wire(); }

    /* The host's answer to one clGetEventInfo. */
    void answers(cl_int status)
    {
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&status), sizeof status);
    }

private:
    ClContextRig                      rig_;
    ClBlockingWait                    waits_;
    av::ObjectStorage<GuestClContext> context_;
};

}  // namespace

TEST(cl_waits, work_already_done_costs_one_round_and_not_none)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  const std::uint32_t commands = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever),
           CL_SUCCESS);
  /* Asked once, which is what makes a zero timeout a poll rather than a
   * refusal. */
  CHECK_EQ(waiting.waits().rounds(), rounds + 1u);
  CHECK_EQ(waiting.rig().kmt().commands(), commands + 1u); }

TEST(cl_waits, a_zero_timeout_still_asks_once)
{ Waiting waiting;
  waiting.answers(CL_COMPLETE);
  const std::uint64_t rounds = waiting.waits().rounds();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 0u), CL_SUCCESS);
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
                                          1000000000u /* one second */),
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
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, 1000000000u),
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
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), both, 2u, SlicedWait::forever),
           CL_SUCCESS);
  CHECK_EQ(waiting.rig().kmt().commands(), commands + 4u); }

TEST(cl_waits, an_event_that_failed_ends_the_wait_with_the_error_the_api_names)
{ Waiting waiting;
  waiting.answers(-5 /* CL_OUT_OF_RESOURCES, as a command's own status */);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST); }

TEST(cl_waits, a_host_that_will_not_answer_the_query_ends_the_wait_with_its_own_error)
{ Waiting waiting;
  waiting.rig().host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 1u, SlicedWait::forever),
           CL_INVALID_EVENT); }

TEST(cl_waits, a_wait_for_nothing_is_the_callers_mistake_and_asks_the_host_nothing)
{ Waiting waiting;
  const std::uint32_t commands = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), nullptr, 1u, SlicedWait::forever),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_complete(*waiting.wire(), &host_event, 0u, SlicedWait::forever),
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
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event),
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
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, host_event),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(waiting.rig().kmt().commands(), first + 1u);
  CHECK_EQ(waiting.waits().rounds(), rounds); }

TEST(cl_waits, a_wait_on_no_queue_or_no_event_is_the_callers_mistake_and_sends_nothing)
{ Waiting waiting;
  const std::uint32_t first = waiting.rig().kmt().commands();
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), 0u, host_event),
           CL_INVALID_VALUE);
  CHECK_EQ(waiting.waits().until_queued_completes(*waiting.wire(), host_queue, 0u),
           CL_INVALID_VALUE);
  /* Not even the flush: neither names a queue this could flush. */
  CHECK_EQ(waiting.rig().kmt().commands(), first);
  CHECK_EQ(waiting.waits().rounds(), 0u); }
