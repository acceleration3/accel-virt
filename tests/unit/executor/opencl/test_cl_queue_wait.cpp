#include "core/platform/poll_clock.hpp"
#include "executor/opencl/cl_queue_wait.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>
#include <cstdint>

namespace {

constexpr std::uint64_t second = 1000000000u;

struct FakeMarker {
    cl_int       status = CL_COMPLETE;
    int          polls_until_done = 0;
    int          polls = 0;
    int          releases = 0;
    cl_int       query_answer = CL_SUCCESS;
};

FakeMarker  marker;
int         markers_enqueued = 0;
int         flushes = 0;
cl_int      enqueue_answer = CL_SUCCESS;

void reset()
{
    marker = FakeMarker();
    markers_enqueued = 0;
    flushes = 0;
    enqueue_answer = CL_SUCCESS;
}

cl_command_queue queue() { return reinterpret_cast<cl_command_queue>(std::uintptr_t{0x40}); }

class StepClock final : public av::PollClock {
public:
    std::uint64_t now_nanoseconds() const override
    {
        now_ += reading_step;
        return now_;
    }
    void          pause(std::uint64_t nanoseconds) override
    {
        now_ += nanoseconds;
        pauses_++;
    }
    static constexpr std::uint64_t reading_step = 100000u;
    mutable std::uint64_t now_ = 1u;
    int           pauses_ = 0;
};

}

extern "C" cl_int clEnqueueMarkerWithWaitList(cl_command_queue, cl_uint, const cl_event *, cl_event *event)
{
    if (enqueue_answer != CL_SUCCESS) return enqueue_answer;
    markers_enqueued++;
    *event = reinterpret_cast<cl_event>(&marker);
    return CL_SUCCESS;
}

extern "C" cl_int clFlush(cl_command_queue)
{
    flushes++;
    return CL_SUCCESS;
}

extern "C" cl_int clGetEventInfo(cl_event, cl_event_info, size_t, void *value, size_t *)
{
    marker.polls++;
    if (marker.query_answer != CL_SUCCESS) return marker.query_answer;
    *static_cast<cl_int *>(value) = marker.polls > marker.polls_until_done ? CL_COMPLETE : CL_RUNNING;
    return CL_SUCCESS;
}

extern "C" cl_int clReleaseEvent(cl_event)
{
    marker.releases++;
    return CL_SUCCESS;
}

TEST(cl_queue_wait, a_queue_that_already_ran_its_work_is_through_after_one_marker_and_one_flush)
{ reset();
  StepClock clock;
  av::ClQueueWait wait(clock);
  CHECK(wait.through(queue(), second));
  CHECK_EQ(markers_enqueued, 1);
  CHECK_EQ(flushes, 1);
  CHECK_EQ(marker.releases, 1);
  CHECK_EQ(clock.pauses_, 0); }

TEST(cl_queue_wait, a_queue_still_running_is_waited_for_by_polling_and_the_marker_is_let_go)
{ reset();
  marker.polls_until_done = 5;
  StepClock clock;
  av::ClQueueWait wait(clock);
  CHECK(wait.through(queue(), second));
  CHECK_EQ(marker.polls, 6);
  CHECK_EQ(marker.releases, 1); }

TEST(cl_queue_wait, a_queue_that_never_finishes_is_given_up_on_at_the_deadline_with_its_marker_let_go)
{ reset();
  marker.polls_until_done = 1000000;
  StepClock clock;
  av::ClQueueWait wait(clock);
  const std::uint64_t began = clock.now_nanoseconds();
  CHECK(!wait.through(queue(), 10000000u));
  CHECK(clock.now_nanoseconds() - began >= 10000000u);
  CHECK(clock.now_nanoseconds() - began < 2u * second);
  CHECK_EQ(marker.releases, 1);
  CHECK_EQ(markers_enqueued, 1); }

TEST(cl_queue_wait, a_queue_that_takes_no_marker_is_not_through)
{ reset();
  enqueue_answer = CL_OUT_OF_RESOURCES;
  StepClock clock;
  av::ClQueueWait wait(clock);
  CHECK(!wait.through(queue(), second));
  CHECK_EQ(flushes, 0);
  CHECK_EQ(marker.releases, 0); }

TEST(cl_queue_wait, a_marker_the_host_can_no_longer_describe_counts_as_done_like_the_drain_takes_it)
{ reset();
  marker.query_answer = CL_INVALID_EVENT;
  StepClock clock;
  av::ClQueueWait wait(clock);
  CHECK(wait.through(queue(), second));
  CHECK_EQ(marker.releases, 1); }
