#include "icd/opencl/cl_blocking_wait.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/counter_wait.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClBlockingWait::ClBlockingWait(Kmt &kmt, ClEventWatcher &watcher)
    : kmt_(kmt), watcher_(watcher), rounds_(0u), raises_asked_(0u)
{
    AV_SCOPE("ClBlockingWait::ClBlockingWait");
}

bool ClBlockingWait::settled(ClientTransport &wire, const uint64_t *events, uint32_t count,
                             cl_int &answer, uint32_t &pending)
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
        if (status != CL_COMPLETE) {
            pending = index;
            return false;
        }
    }
    answer = CL_SUCCESS;
    return true;
}

/* What the wait was woken by is read before it asks: a wake that comes after
 * the host was asked - its raise on the very event this then finds pending -
 * is past what was seen, so the sleep that follows ends at once rather than
 * missing it. The raise is asked for after the round that found the event
 * pending, and an event already finished by then has its callback run at
 * once, raising all the same. A host that will not raise on one, or a watcher
 * with no thread to tell this, leaves the wait asking on a timer.
 *
 * Neither happens in the first millisecond, which asks back to back: the wake
 * is the host's callback, its worker, an interrupt, a DPC and two threads
 * scheduled, and work that ends that soon is seen sooner by asking - which is
 * what most of Affinity's drains are. */
cl_int ClBlockingWait::until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                                      uint64_t timeout, bool driven)
{
    AV_SCOPE("ClBlockingWait::until_complete");
    if (events == nullptr || count == 0u) return CL_INVALID_VALUE;
    icd::CounterWait slices(kmt_, timeout);
    uint64_t         slice = 0u;
    cl_int           answer = CL_SUCCESS;
    uint64_t         frequency = 0u;
    const uint64_t   started = kmt_.performance_counter(frequency);
    bool             woken_by_driver = driven && watcher_.attend();
    uint64_t         raising = 0u;
    while (slices.next(slice)) {
        const uint64_t seen = woken_by_driver ? watcher_.woken() : 0u;
        uint32_t       pending = 0u;
        if (settled(wire, events, count, answer, pending)) return answer;
        /* Back to back for the first millisecond. Past it a round trip each
         * few microseconds is the transport and a processor taken from the
         * application for nothing - a drain behind a kernel asked over a
         * thousand times in 4 ms. */
        uint64_t unused = 0u;
        if (frequency != 0u && kmt_.performance_counter(unused) - started < frequency / spin_per_second) continue;
        if (woken_by_driver && events[pending] != raising) {
            raising = events[pending];
            ++raises_asked_;
            woken_by_driver = av::clproto::av_clSetEventCallback(wire, raising, CL_COMPLETE) == CL_SUCCESS;
        }
        if (woken_by_driver) {
            watcher_.await_woken(seen, slice);
            continue;
        }
        kmt_.sleep_briefly();
    }
    /* The deadline passed with work still outstanding. The two APIs this
     * serves have no timeout to report, so what the caller gets is the answer
     * a completed wait would give; the events say the rest. */
    return CL_SUCCESS;
}

cl_int ClBlockingWait::until_queued_completes(ClientTransport &wire, uint64_t queue,
                                              uint64_t event, bool driven)
{
    AV_SCOPE("ClBlockingWait::until_queued_completes");
    if (queue == 0u || event == 0u) return CL_INVALID_VALUE;
    const cl_int flushed = av::clproto::av_clFlush(wire, queue);
    if (flushed != CL_SUCCESS) return flushed;
    return until_complete(wire, &event, 1u, icd::SlicedWait::forever, driven);
}

uint64_t ClBlockingWait::rounds() const
{
    AV_SCOPE("ClBlockingWait::rounds");
    return rounds_;
}

uint64_t ClBlockingWait::raises_asked() const
{
    AV_SCOPE("ClBlockingWait::raises_asked");
    return raises_asked_;
}

}  // namespace av::clicd
