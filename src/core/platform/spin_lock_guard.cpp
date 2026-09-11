#include "core/platform/spin_lock_guard.hpp"

#include "core/platform/spin_lock.hpp"

namespace av {

SpinLockGuard::SpinLockGuard(SpinLock &lock) : lock_(lock), previous_(lock.acquire()) {}

SpinLockGuard::~SpinLockGuard() { lock_.release(previous_); }

}  // namespace av
