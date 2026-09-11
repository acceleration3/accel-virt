/* One acquisition of a SpinLock, released when the scope ends. */
#ifndef AV_SPIN_LOCK_GUARD_HPP
#define AV_SPIN_LOCK_GUARD_HPP

#include "core/platform/platform.hpp"

namespace av {

class SpinLock;

class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock &lock);
    ~SpinLockGuard();
    SpinLockGuard(const SpinLockGuard &) = delete;
    SpinLockGuard &operator=(const SpinLockGuard &) = delete;

private:
    SpinLock &lock_;
    Irql      previous_;
};

}  // namespace av

#endif  // AV_SPIN_LOCK_GUARD_HPP
