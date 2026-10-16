#include "icd/d3d/session_timeline.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::d3d {

SessionTimeline::SessionTimeline(icd::HostChannel &channel, ThreadLock &lock) : timeline_(channel), lock_(lock)
{
    AV_SCOPE("SessionTimeline::SessionTimeline");
}

uint64_t SessionTimeline::recording() const
{
    AV_SCOPE("SessionTimeline::recording");
    return timeline_.recording();
}

uint64_t SessionTimeline::completed() const
{
    AV_SCOPE("SessionTimeline::completed");
    return timeline_.completed();
}

bool SessionTimeline::wait(uint64_t fence)
{
    AV_SCOPE("SessionTimeline::wait");
    const ThreadLockHold hold(lock_);
    return timeline_.wait(fence);
}

}  // namespace av::d3d
