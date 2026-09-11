/* A lock for PASSIVE-level state whose critical section may allocate or block.
 * Holding it raises to APC; it must never be taken at DISPATCH. Take it through
 * a MutexGuard so no return path leaves it held. */
#ifndef AV_MUTEX_HPP
#define AV_MUTEX_HPP

#include "lock_rank.hpp"
#include "platform.hpp"

#include <stddef.h>

namespace av {

class Mutex {
public:
    /* Opaque room for the kernel's FAST_MUTEX (56 bytes on x64); src/km proves
     * it fits at compile time. */
    static constexpr size_t storage_bytes = 64u;

    Mutex(Platform &platform, LockRank rank);
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    void acquire();
    void release();

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
    LockRank  rank_;
};

}  // namespace av

#endif  // AV_MUTEX_HPP
