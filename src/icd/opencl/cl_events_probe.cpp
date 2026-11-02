#include "icd/opencl/cl_events_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClEventsProbe::ClEventsProbe(ClientTransport &wire, Kmt &kmt, ClEventWatcher &watcher, const uint64_t *events,
                             uint32_t count, bool driven, uint64_t &rounds, uint64_t &raises)
    : wire_(wire), polls_(kmt), watcher_(watcher), events_(events), count_(count), rounds_(rounds), raises_(raises),
      woken_by_driver_(driven && watcher.attend()),
      seen_(0u), pending_(0u), raising_(0u), answer_(CL_SUCCESS)
{
    AV_SCOPE("ClEventsProbe::ClEventsProbe");
}

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
        if (asked != CL_SUCCESS) {
            answer_ = asked;
            return true;
        }
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

void ClEventsProbe::pause(uint64_t slice)
{
    AV_SCOPE("ClEventsProbe::pause");
    if (polls_.spinning()) return;
    if (woken_by_driver_ && events_[pending_] != raising_) {
        raising_ = events_[pending_];
        ++raises_;
        woken_by_driver_ = av::clproto::av_clSetEventCallback(wire_, raising_, CL_COMPLETE) == CL_SUCCESS;
    }
    if (woken_by_driver_) {
        watcher_.await_woken(seen_, slice);
        return;
    }
    polls_.rest();
}

cl_int ClEventsProbe::answer() const
{
    AV_SCOPE("ClEventsProbe::answer");
    return answer_;
}

}
