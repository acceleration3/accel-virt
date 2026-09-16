/* A lock for PASSIVE-level work that waits while holding it: a control request
 * sequence stalls between polls, which a fast mutex forbids by raising to APC.
 * Holding it keeps the IRQL where it was (the kernel's KMUTEX); it must never be
 * taken above PASSIVE. Take it through a WaitMutexGuard. */
#ifndef AV_WAIT_MUTEX_HPP
#define AV_WAIT_MUTEX_HPP

#include "core/platform/lock_marks.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"

#include <stddef.h>

namespace av {

class WaitMutex {
public:
    /* Opaque room for the kernel's KMUTEX (56 bytes on x64); src/km proves it
     * fits at compile time. */
    static constexpr size_t storage_bytes = 64u;

    WaitMutex(Platform &platform, LockRank rank);
    WaitMutex(const WaitMutex &) = delete;
    WaitMutex &operator=(const WaitMutex &) = delete;

    void acquire();
    /* The lock if it is free, without waiting; whether it was taken. */
    [[nodiscard]] bool try_acquire();
    void release();

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
    LockRank  rank_;
    LockMarks marks_;
};

}  // namespace av

#endif  // AV_WAIT_MUTEX_HPP
