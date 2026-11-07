#include "executor/opencl/cl_user_events.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClUserEvents::ClUserEvents() : events_{}, count_(0u)
{
    AV_SCOPE("ClUserEvents::ClUserEvents");
}

void ClUserEvents::track(cl_event event)
{
    AV_SCOPE("ClUserEvents::track");
    if (event == nullptr) return;
    if (count_ == most) purge();
    if (count_ == most || ::clRetainEvent(event) != CL_SUCCESS) return;
    events_[count_++] = event;
}

void ClUserEvents::settled(cl_event event)
{
    AV_SCOPE("ClUserEvents::settled");
    for (uint32_t index = 0u; index < count_; index++) {
        if (events_[index] != event) continue;
        drop(index);
        return;
    }
}

uint32_t ClUserEvents::fail_pending()
{
    AV_SCOPE("ClUserEvents::fail_pending");
    uint32_t failed = 0u;
    while (count_ != 0u) {
        const cl_event event = events_[count_ - 1u];
        cl_int         status = CL_COMPLETE;
        if (::clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr) == CL_SUCCESS &&
            status == CL_SUBMITTED && ::clSetUserEventStatus(event, CL_INVALID_OPERATION) == CL_SUCCESS)
            failed++;
        drop(count_ - 1u);
    }
    return failed;
}

uint32_t ClUserEvents::count() const
{
    AV_SCOPE("ClUserEvents::count");
    return count_;
}

void ClUserEvents::drop(uint32_t index)
{
    AV_SCOPE("ClUserEvents::drop");
    static_cast<void>(::clReleaseEvent(events_[index]));
    events_[index] = events_[--count_];
    events_[count_] = nullptr;
}

void ClUserEvents::purge()
{
    AV_SCOPE("ClUserEvents::purge");
    for (uint32_t index = count_; index-- > 0u;) {
        cl_int status = CL_SUBMITTED;
        if (::clGetEventInfo(events_[index], CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr) !=
                CL_SUCCESS ||
            status != CL_SUBMITTED)
            drop(index);
    }
}

}
