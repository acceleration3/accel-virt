#ifndef AV_WAIT_MUTEX_HPP
#define AV_WAIT_MUTEX_HPP

#include "core/platform/lock_marks.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"

#include <stddef.h>

namespace av {

class WaitMutex {
public:
    static constexpr size_t storage_bytes = 64u;

    WaitMutex(Platform &platform, LockRank rank);
    WaitMutex(const WaitMutex &) = delete;
    WaitMutex &operator=(const WaitMutex &) = delete;

    void acquire();
    void release();

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
    LockRank  rank_;
    LockMarks marks_;
};

}

#endif
