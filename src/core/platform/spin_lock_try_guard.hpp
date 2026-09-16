/* One attempt at a SpinLock that does not wait, released when the scope ends if it
 * was taken: how a state dump reads what a lock guards without ever blocking
 * on a thread that holds it. */
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

}  // namespace av

#endif  // AV_SPIN_LOCK_TRY_GUARD_HPP
