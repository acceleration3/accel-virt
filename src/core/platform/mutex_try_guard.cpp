#include "core/platform/mutex_try_guard.hpp"

#include "core/platform/mutex.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

MutexTryGuard::MutexTryGuard(Mutex &lock) : lock_(lock), held_(lock.try_acquire())
{
    AV_SCOPE("MutexTryGuard::MutexTryGuard");
}

MutexTryGuard::~MutexTryGuard()
{
    AV_SCOPE("MutexTryGuard::~MutexTryGuard");
    if (held_) lock_.release();
}

bool MutexTryGuard::held() const
{
    AV_SCOPE("MutexTryGuard::held");
    return held_;
}

}  // namespace av
