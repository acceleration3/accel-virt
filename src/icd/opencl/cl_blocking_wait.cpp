#include "icd/opencl/cl_blocking_wait.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/sliced_wait.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClBlockingWait::ClBlockingWait(Kmt &kmt) : kmt_(kmt), rounds_(0u)
{
    AV_SCOPE("ClBlockingWait::ClBlockingWait");
}

bool ClBlockingWait::settled(ClientTransport &wire, const uint64_t *events, uint32_t count,
                             cl_int &answer)
{
    AV_SCOPE("ClBlockingWait::settled");
    ++rounds_;
    for (uint32_t index = 0u; index < count; ++index) {
        cl_int       status = CL_QUEUED;
        size_t       answered = 0u;
        const cl_int asked = av::clproto::av_clGetEventInfo(
            wire, events[index], CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
            &answered);
        if (asked != CL_SUCCESS) {
            /* An event the host will not answer about is one this wait can
             * never end on; the caller is told rather than left here. */
            answer = asked;
            return true;
        }
        /* A negative status is the command's own failure, which ends the wait
         * with the error the API names for it rather than another round. */
        if (status < 0) {
            answer = CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            return true;
        }
        if (status != CL_COMPLETE) return false;
    }
    answer = CL_SUCCESS;
    return true;
}

cl_int ClBlockingWait::until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                                      uint64_t timeout)
{
    AV_SCOPE("ClBlockingWait::until_complete");
    if (events == nullptr || count == 0u) return CL_INVALID_VALUE;
    uint64_t        frequency = 0u;
    const uint64_t  start = kmt_.performance_counter(frequency);
    icd::SlicedWait slices(timeout, start, frequency);
    uint64_t        slice = 0u;
    cl_int          answer = CL_SUCCESS;
    while (slices.next(kmt_.performance_counter(frequency), slice)) {
        if (settled(wire, events, count, answer)) return answer;
    }
    /* The deadline passed with work still outstanding. The two APIs this
     * serves have no timeout to report, so what the caller gets is the answer
     * a completed wait would give; the events say the rest. */
    return CL_SUCCESS;
}

cl_int ClBlockingWait::until_queued_completes(ClientTransport &wire, uint64_t queue,
                                              uint64_t event)
{
    AV_SCOPE("ClBlockingWait::until_queued_completes");
    if (queue == 0u || event == 0u) return CL_INVALID_VALUE;
    const cl_int flushed = av::clproto::av_clFlush(wire, queue);
    if (flushed != CL_SUCCESS) return flushed;
    return until_complete(wire, &event, 1u, icd::SlicedWait::forever);
}

uint64_t ClBlockingWait::rounds() const
{
    AV_SCOPE("ClBlockingWait::rounds");
    return rounds_;
}

}  // namespace av::clicd
