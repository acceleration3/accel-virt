#include "core/platform/spin_lock_guard.hpp"

#include "core/platform/spin_lock.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SpinLockGuard::SpinLockGuard(SpinLock &lock) : lock_(lock), previous_(lock.acquire())
{
    AV_SCOPE("SpinLockGuard::SpinLockGuard");
}

SpinLockGuard::~SpinLockGuard()
{
    AV_SCOPE("SpinLockGuard::~SpinLockGuard");
    lock_.release(previous_);
}

}  // namespace av
