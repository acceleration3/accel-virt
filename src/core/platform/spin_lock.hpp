#ifndef AV_SPIN_LOCK_HPP
#define AV_SPIN_LOCK_HPP

#include "core/platform/lock_marks.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class SpinLock {
public:
    SpinLock(Platform &platform, LockRank rank);
    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;

    [[nodiscard]] Irql acquire();
    [[nodiscard]] bool try_acquire(Irql &previous);
    void release(Irql previous);

private:
    Platform &platform_;
    uintptr_t word_;
    LockRank  rank_;
    LockMarks marks_;
};

}

#endif
