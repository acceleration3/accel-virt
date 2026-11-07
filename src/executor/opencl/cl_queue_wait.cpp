#include "executor/opencl/cl_queue_wait.hpp"

#include "core/platform/poll_budget.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

ClQueueWait::ClQueueWait(PollClock &clock) : clock_(clock)
{
    AV_SCOPE("ClQueueWait::ClQueueWait");
}

bool ClQueueWait::through(cl_command_queue queue, uint64_t nanoseconds)
{
    AV_SCOPE("ClQueueWait::through");
    constexpr uint64_t spin_nanoseconds = 2000000u;
    constexpr uint64_t pause_nanoseconds = 50000u;
    cl_event           marker = nullptr;
    if (::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, &marker) != CL_SUCCESS || marker == nullptr) return false;
    static_cast<void>(::clFlush(queue));
    bool       done = false;
    PollBudget polls(clock_, spin_nanoseconds, pause_nanoseconds, nanoseconds);
    while (!done && polls.next()) {
        cl_int status = CL_QUEUED;
        done = ::clGetEventInfo(marker, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr) != CL_SUCCESS ||
               status <= CL_COMPLETE;
    }
    static_cast<void>(::clReleaseEvent(marker));
    return done;
}

}
