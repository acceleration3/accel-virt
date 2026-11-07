#include "core/log/driver_log.hpp"
#include "executor/executor_log.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/teardown_budget.hpp"
#include "harness/executor/generous_budget.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <CL/cl.h>
#include <cstdint>

namespace {

constexpr std::uint32_t most_marked = 16u;
constexpr std::uint32_t most_events = 16u;

struct FakeEvent {
    cl_int status = CL_COMPLETE;
    bool   user = false;
    int    retains = 0;
    cl_int set_to = 0;
    bool   was_set = false;
};

FakeEvent        events[most_events];
std::uint32_t    made_events = 0u;
cl_command_queue marked[most_marked];
std::uint32_t    marked_count = 0u;
cl_int           marker_answer = CL_SUCCESS;
cl_int           marker_status = CL_COMPLETE;

void forget_marked()
{
    marked_count = 0u;
    marker_answer = CL_SUCCESS;
    marker_status = CL_COMPLETE;
    made_events = 0u;
    for (FakeEvent &event : events) event = FakeEvent();
}

std::uint32_t times_marked(cl_command_queue queue)
{
    std::uint32_t times = 0u;
    for (std::uint32_t at = 0u; at < marked_count; at++)
        if (marked[at] == queue) times++;
    return times;
}

cl_command_queue fake_queue(std::uintptr_t number) { return reinterpret_cast<cl_command_queue>(number); }

FakeEvent *as_event(cl_event event) { return reinterpret_cast<FakeEvent *>(event); }

av::RecordingLog sink;
av::ExecutorLog  executor_log(sink);

}

extern "C" cl_int clEnqueueMarkerWithWaitList(cl_command_queue queue, cl_uint, const cl_event *, cl_event *event)
{
    if (marked_count < most_marked) marked[marked_count++] = queue;
    if (marker_answer != CL_SUCCESS) return marker_answer;
    FakeEvent &made = events[made_events++];
    made.status = marker_status;
    *event = reinterpret_cast<cl_event>(&made);
    return CL_SUCCESS;
}

extern "C" cl_int clFlush(cl_command_queue) { return CL_SUCCESS; }

extern "C" cl_int clGetEventInfo(cl_event event, cl_event_info what, size_t, void *value, size_t *)
{
    if (what == CL_EVENT_COMMAND_EXECUTION_STATUS) *static_cast<cl_int *>(value) = as_event(event)->status;
    return CL_SUCCESS;
}

extern "C" cl_int clRetainEvent(cl_event event)
{
    as_event(event)->retains++;
    return CL_SUCCESS;
}

extern "C" cl_int clReleaseEvent(cl_event event)
{
    as_event(event)->retains--;
    return CL_SUCCESS;
}

extern "C" cl_event clCreateUserEvent(cl_context, cl_int *error)
{
    FakeEvent &made = events[made_events++];
    made.status = CL_SUBMITTED;
    made.user = true;
    made.retains = 1;
    *error = CL_SUCCESS;
    return reinterpret_cast<cl_event>(&made);
}

extern "C" cl_int clSetUserEventStatus(cl_event event, cl_int status)
{
    FakeEvent *const set = as_event(event);
    set->was_set = true;
    set->set_to = status;
    set->status = status;
    return CL_SUCCESS;
}

namespace av {
struct OpenClBackendProbe {
    static void borrow_queue(OpenClBackend &backend, void *queue) { backend.queue_ = queue; }
    static void keep(OpenClBackend &backend, clproto::HandleKind kind, void *object)
    {
        CHECK_STATUS(backend.hold(kind, object), Status::ok);
    }
};
}

using av::OpenClBackend;
using av::OpenClBackendProbe;
using av::clproto::HandleKind;

TEST(opencl_backend_drain, marks_nothing_for_a_backend_that_holds_no_queue)
{ forget_marked();
  OpenClBackend backend(executor_log.opencl());
  av::GenerousBudget budget;
  CHECK(backend.drain(budget.budget()));
  CHECK_EQ(marked_count, 0u); }

TEST(opencl_backend_drain, waits_on_the_queue_the_backend_made_and_every_queue_the_guest_holds_once_each)
{ forget_marked();
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x2000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x3000u));
  av::GenerousBudget budget;
  CHECK(backend.drain(budget.budget()));
  CHECK_EQ(marked_count, 3u);
  CHECK_EQ(times_marked(fake_queue(0x1000u)), 1u);
  CHECK_EQ(times_marked(fake_queue(0x2000u)), 1u);
  CHECK_EQ(times_marked(fake_queue(0x3000u)), 1u);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }

TEST(opencl_backend_drain, leaves_alone_every_held_object_that_is_not_a_command_queue)
{ forget_marked();
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::keep(backend, HandleKind::mem, fake_queue(0x4000u));
  OpenClBackendProbe::keep(backend, HandleKind::context, fake_queue(0x5000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x6000u));
  av::GenerousBudget budget;
  CHECK(backend.drain(budget.budget()));
  CHECK_EQ(marked_count, 1u);
  CHECK_EQ(times_marked(fake_queue(0x6000u)), 1u);
  CHECK_EQ(times_marked(fake_queue(0x4000u)), 0u);
  CHECK_EQ(times_marked(fake_queue(0x5000u)), 0u); }

TEST(opencl_backend_drain, goes_on_to_the_next_queue_when_one_refuses_its_marker_and_reports_the_drain_unfinished)
{ forget_marked();
  marker_answer = CL_OUT_OF_RESOURCES;
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x2000u));
  av::GenerousBudget budget;
  CHECK(!backend.drain(budget.budget()));
  CHECK_EQ(marked_count, 2u);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }

TEST(opencl_backend_drain, a_queue_that_never_finishes_ends_the_drain_at_the_budget_and_reports_it_unfinished)
{ forget_marked();
  marker_status = CL_RUNNING;
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  av::MonotonicClock clock;
  av::TeardownBudget budget(clock, 20u * 1000u * 1000u);
  const std::uint64_t started = clock.now_nanoseconds();
  CHECK(!backend.drain(budget));
  CHECK(clock.now_nanoseconds() - started < 1000u * 1000u * 1000u);
  CHECK_EQ(events[0].retains, -1);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }

TEST(opencl_backend_drain, a_user_event_the_guest_never_set_is_failed_so_the_queues_behind_it_can_finish)
{ forget_marked();
  OpenClBackend backend(executor_log.opencl());
  cl_int error = CL_INVALID_VALUE;
  cl_event const waiting = backend.on_clCreateUserEvent(nullptr, &error);
  cl_event const answered = backend.on_clCreateUserEvent(nullptr, &error);
  CHECK_EQ(backend.on_clSetUserEventStatus(answered, CL_COMPLETE), CL_SUCCESS);
  CHECK_EQ(as_event(answered)->retains, 1);
  CHECK_EQ(as_event(waiting)->retains, 2);
  av::GenerousBudget budget;
  CHECK(backend.drain(budget.budget()));
  CHECK(as_event(waiting)->was_set);
  CHECK(as_event(waiting)->set_to < 0);
  CHECK(as_event(answered)->set_to == CL_COMPLETE);
  CHECK_EQ(as_event(waiting)->retains, 1);
  CHECK_EQ(as_event(answered)->retains, 1);
  as_event(waiting)->retains = 1; }

TEST(opencl_backend_drain, a_user_event_is_failed_before_any_queue_is_waited_on)
{ forget_marked();
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  cl_int error = CL_INVALID_VALUE;
  cl_event const waiting = backend.on_clCreateUserEvent(nullptr, &error);
  marker_status = CL_RUNNING;
  av::MonotonicClock clock;
  av::TeardownBudget budget(clock, 5u * 1000u * 1000u);
  CHECK(!backend.drain(budget));
  CHECK(as_event(waiting)->was_set);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }
