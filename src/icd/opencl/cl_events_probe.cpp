#include "icd/opencl/cl_events_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClEventsProbe::ClEventsProbe(ClientTransport &wire, Kmt &kmt, ClEventWatcher &watcher, const uint64_t *events,
                             uint32_t count, bool driven, uint64_t &rounds, uint64_t &raises)
    : wire_(wire), kmt_(kmt), watcher_(watcher), events_(events), count_(count), rounds_(rounds), raises_(raises),
      frequency_(0u), started_(kmt.performance_counter(frequency_)), woken_by_driver_(driven && watcher.attend()),
      seen_(0u), pending_(0u), raising_(0u), answer_(CL_SUCCESS)
{
    AV_SCOPE("ClEventsProbe::ClEventsProbe");
}

/* What the wait was woken by is read before it asks: a wake that comes after
 * the host was asked - its raise on the very event this then finds pending -
 * is past what was seen, so the pause that follows ends at once rather than
 * missing it. */
bool ClEventsProbe::ask(uint64_t)
{
    AV_SCOPE("ClEventsProbe::ask");
    seen_ = woken_by_driver_ ? watcher_.woken() : 0u;
    ++rounds_;
    for (uint32_t index = 0u; index < count_; ++index) {
        cl_int       status = CL_QUEUED;
        size_t       answered = 0u;
        const cl_int asked = av::clproto::av_clGetEventInfo(wire_, events_[index], CL_EVENT_COMMAND_EXECUTION_STATUS,
                                                            sizeof status, &status, &answered);
        /* An event the host will not answer about is one this wait can never
         * end on; the caller is told rather than left here. */
        if (asked != CL_SUCCESS) {
            answer_ = asked;
            return true;
        }
        /* A negative status is the command's own failure, which ends the wait
         * with the error the API names for it rather than another round. */
        if (status < 0) {
            answer_ = CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            return true;
        }
        if (status != CL_COMPLETE) {
            pending_ = index;
            return false;
        }
    }
    answer_ = CL_SUCCESS;
    return true;
}

/* Back to back for the first millisecond: the wake is the host's callback, its
 * worker, an interrupt, a DPC and two threads scheduled, and work that ends
 * that soon is seen sooner by asking - which is what most of Affinity's drains
 * are. Past it a round trip each few microseconds is the transport and a
 * processor taken from the application for nothing - a drain behind a kernel
 * asked over a thousand times in 4 ms. The raise is asked for after the round
 * that found the event pending, and an event already finished by then has its
 * callback run at once, raising all the same. A host that will not raise on
 * one, or a watcher with no thread to tell this, leaves the wait asking on a
 * timer. */
void ClEventsProbe::pause(uint64_t slice)
{
    AV_SCOPE("ClEventsProbe::pause");
    uint64_t unused = 0u;
    if (frequency_ != 0u && kmt_.performance_counter(unused) - started_ < frequency_ / spin_per_second) return;
    if (woken_by_driver_ && events_[pending_] != raising_) {
        raising_ = events_[pending_];
        ++raises_;
        woken_by_driver_ = av::clproto::av_clSetEventCallback(wire_, raising_, CL_COMPLETE) == CL_SUCCESS;
    }
    if (woken_by_driver_) {
        watcher_.await_woken(seen_, slice);
        return;
    }
    kmt_.sleep_briefly();
}

cl_int ClEventsProbe::answer() const
{
    AV_SCOPE("ClEventsProbe::answer");
    return answer_;
}

}  // namespace av::clicd
