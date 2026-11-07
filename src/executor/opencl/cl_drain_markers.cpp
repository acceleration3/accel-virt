#include "executor/opencl/cl_drain_markers.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/teardown_budget.hpp"

namespace av {

ClDrainMarkers::ClDrainMarkers(BlockSource &blocks) : rows_(blocks, Pool::paged)
{
    AV_SCOPE("ClDrainMarkers::ClDrainMarkers");
}

ClDrainMarkers::~ClDrainMarkers()
{
    AV_SCOPE("ClDrainMarkers::~ClDrainMarkers");
    release_all();
}

bool ClDrainMarkers::reached(const Row &row) const
{
    AV_SCOPE("ClDrainMarkers::reached");
    cl_int status = CL_QUEUED;
    return ::clGetEventInfo(static_cast<cl_event>(row.event_), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status,
                            &status, nullptr) != CL_SUCCESS ||
           status <= CL_COMPLETE;
}

bool ClDrainMarkers::settle(cl_command_queue queue, uint64_t work, MonotonicClock &clock, TeardownBudget &budget)
{
    AV_SCOPE("ClDrainMarkers::settle");
    constexpr uint64_t poll_nanoseconds = 100000u;
    const uint64_t     key = reinterpret_cast<uintptr_t>(queue);
    Row *kept = rows_.find(key);
    if (kept != nullptr && kept->work_ != work) {
        static_cast<void>(::clReleaseEvent(static_cast<cl_event>(kept->event_)));
        static_cast<void>(rows_.erase(key));
        kept = nullptr;
    }
    if (kept == nullptr) {
        cl_event marker = nullptr;
        if (::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, &marker) != CL_SUCCESS || marker == nullptr) return false;
        uint32_t slot = 0u;
        if (rows_.insert(key, Row{marker, work}, slot) != Status::ok) {
            static_cast<void>(::clReleaseEvent(marker));
            return false;
        }
        kept = rows_.find(key);
    }
    static_cast<void>(::clFlush(queue));
    bool done = false;
    for (;;) {
        done = reached(*kept);
        if (done || budget.expired()) break;
        clock.pause(poll_nanoseconds);
    }
    if (!done) return false;
    static_cast<void>(::clReleaseEvent(static_cast<cl_event>(kept->event_)));
    static_cast<void>(rows_.erase(key));
    return true;
}

void ClDrainMarkers::release_all()
{
    AV_SCOPE("ClDrainMarkers::release_all");
    rows_.erase_if([](uint64_t, const Row &row) {
        static_cast<void>(::clReleaseEvent(static_cast<cl_event>(row.event_)));
        return true;
    });
}

uint32_t ClDrainMarkers::count() const
{
    AV_SCOPE("ClDrainMarkers::count");
    return rows_.count();
}

}
