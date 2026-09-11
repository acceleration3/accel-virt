/* A lock for state reachable at DISPATCH_LEVEL. Holding it raises to DISPATCH:
 * nothing under it may allocate, block, or take a mutex. Take it through a
 * SpinLockGuard so no return path leaves it held. */
#ifndef AV_SPIN_LOCK_HPP
#define AV_SPIN_LOCK_HPP

#include "lock_rank.hpp"
#include "platform.hpp"

#include <stdint.h>

namespace av {

class SpinLock {
public:
    SpinLock(Platform &platform, LockRank rank);
    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;

    /* Returns the level release() must restore. */
    [[nodiscard]] Irql acquire();
    void release(Irql previous);

private:
    Platform &platform_;
    uintptr_t word_;
    LockRank  rank_;
};

}  // namespace av

#endif  // AV_SPIN_LOCK_HPP
