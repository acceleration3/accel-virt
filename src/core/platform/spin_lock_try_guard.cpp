#include "core/platform/spin_lock_try_guard.hpp"

#include "core/platform/spin_lock.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SpinLockTryGuard::SpinLockTryGuard(SpinLock &lock) : lock_(lock), previous_(Irql::passive), held_(lock.try_acquire(previous_))
{
    AV_SCOPE("SpinLockTryGuard::SpinLockTryGuard");
}

SpinLockTryGuard::~SpinLockTryGuard()
{
    AV_SCOPE("SpinLockTryGuard::~SpinLockTryGuard");
    if (held_) lock_.release(previous_);
}

bool SpinLockTryGuard::held() const
{
    AV_SCOPE("SpinLockTryGuard::held");
    return held_;
}

}  // namespace av
