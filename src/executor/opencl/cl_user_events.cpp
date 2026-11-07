#include "executor/opencl/cl_user_events.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t key_of(cl_event event)
{
    AV_SCOPE("key_of");
    return reinterpret_cast<uintptr_t>(event);
}

}

ClUserEvents::ClUserEvents(BlockSource &blocks) : events_(blocks, Pool::paged)
{
    AV_SCOPE("ClUserEvents::ClUserEvents");
}

ClUserEvents::~ClUserEvents()
{
    AV_SCOPE("ClUserEvents::~ClUserEvents");
    static_cast<void>(fail_pending());
}

void ClUserEvents::track(cl_event event)
{
    AV_SCOPE("ClUserEvents::track");
    if (event == nullptr || events_.find(key_of(event)) != nullptr || ::clRetainEvent(event) != CL_SUCCESS) return;
    uint32_t slot = 0u;
    if (events_.insert(key_of(event), true, slot) != Status::ok) static_cast<void>(::clReleaseEvent(event));
}

void ClUserEvents::settled(cl_event event)
{
    AV_SCOPE("ClUserEvents::settled");
    if (events_.erase(key_of(event))) static_cast<void>(::clReleaseEvent(event));
}

uint32_t ClUserEvents::fail_pending()
{
    AV_SCOPE("ClUserEvents::fail_pending");
    uint32_t failed = 0u;
    events_.erase_if([&failed](uint64_t key, const bool &) {
        const cl_event event = reinterpret_cast<cl_event>(static_cast<uintptr_t>(key));
        cl_int         status = CL_COMPLETE;
        if (::clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr) == CL_SUCCESS &&
            status == CL_SUBMITTED && ::clSetUserEventStatus(event, CL_INVALID_OPERATION) == CL_SUCCESS)
            failed++;
        static_cast<void>(::clReleaseEvent(event));
        return true;
    });
    return failed;
}

uint32_t ClUserEvents::count() const
{
    AV_SCOPE("ClUserEvents::count");
    return events_.count();
}

}
