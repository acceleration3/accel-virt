#include "icd/common/locked_timeline.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

LockedTimeline::LockedTimeline(HostChannel &channel, ThreadLock &lock) : timeline_(channel), lock_(lock)
{
    AV_SCOPE("LockedTimeline::LockedTimeline");
}

uint64_t LockedTimeline::recording() const
{
    AV_SCOPE("LockedTimeline::recording");
    return timeline_.recording();
}

uint64_t LockedTimeline::completed() const
{
    AV_SCOPE("LockedTimeline::completed");
    return timeline_.completed();
}

bool LockedTimeline::wait(uint64_t fence)
{
    AV_SCOPE("LockedTimeline::wait");
    uint64_t target = 0u;
    {
        const ThreadLockHold hold(lock_);
        if (!timeline_.publish(fence, target)) return false;
    }
    return timeline_.await(target);
}

void LockedTimeline::forget()
{
    AV_SCOPE("LockedTimeline::forget");
    timeline_.forget();
}

}
