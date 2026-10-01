#include "icd/common/thread_lock_hold.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::icd {

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

}  // namespace av::icd
