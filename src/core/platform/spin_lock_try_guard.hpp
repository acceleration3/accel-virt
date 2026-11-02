#ifndef AV_SPIN_LOCK_TRY_GUARD_HPP
#define AV_SPIN_LOCK_TRY_GUARD_HPP

#include "core/platform/platform.hpp"

namespace av {

class SpinLock;

class SpinLockTryGuard {
public:
    explicit SpinLockTryGuard(SpinLock &lock);
    ~SpinLockTryGuard();
    SpinLockTryGuard(const SpinLockTryGuard &) = delete;
    SpinLockTryGuard &operator=(const SpinLockTryGuard &) = delete;

    bool held() const;

private:
    SpinLock &lock_;
    Irql      previous_;
    bool      held_;
};

}

#endif
