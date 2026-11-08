#include "icd/d3d/clock_resync_schedule.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

ClockResyncSchedule::ClockResyncSchedule() : last_(0u), any_(false)
{
    AV_SCOPE("ClockResyncSchedule::ClockResyncSchedule");
}

bool ClockResyncSchedule::due(uint64_t now_nanoseconds) const
{
    AV_SCOPE("ClockResyncSchedule::due");
    if (!any_) return true;
    return now_nanoseconds < last_ || now_nanoseconds - last_ >= interval_nanoseconds;
}

void ClockResyncSchedule::taken(uint64_t now_nanoseconds)
{
    AV_SCOPE("ClockResyncSchedule::taken");
    last_ = now_nanoseconds;
    any_ = true;
}

bool ClockResyncSchedule::invariants_hold() const
{
    AV_SCOPE("ClockResyncSchedule::invariants_hold");
    return any_ || last_ == 0u;
}

}
