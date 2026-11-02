#include "core/platform/thread_lock_hold.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

ThreadLockHold::ThreadLockHold(ThreadLock &lock) : lock_(lock)
{
    AV_SCOPE("ThreadLockHold::ThreadLockHold");
    lock_.acquire();
}

ThreadLockHold::~ThreadLockHold()
{
    AV_SCOPE("ThreadLockHold::~ThreadLockHold");
    lock_.release();
}

}
