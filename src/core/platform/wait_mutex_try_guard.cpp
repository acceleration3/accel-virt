#include "core/platform/wait_mutex_try_guard.hpp"

#include "core/platform/wait_mutex.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

WaitMutexTryGuard::WaitMutexTryGuard(WaitMutex &lock) : lock_(lock), held_(lock.try_acquire())
{
    AV_SCOPE("WaitMutexTryGuard::WaitMutexTryGuard");
}

WaitMutexTryGuard::~WaitMutexTryGuard()
{
    AV_SCOPE("WaitMutexTryGuard::~WaitMutexTryGuard");
    if (held_) lock_.release();
}

bool WaitMutexTryGuard::held() const
{
    AV_SCOPE("WaitMutexTryGuard::held");
    return held_;
}

}  // namespace av
