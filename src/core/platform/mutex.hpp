#ifndef AV_MUTEX_HPP
#define AV_MUTEX_HPP

#include "core/platform/lock_marks.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"

#include <stddef.h>

namespace av {

class Mutex {
public:
    static constexpr size_t storage_bytes = 64u;

    Mutex(Platform &platform, LockRank rank);
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    void acquire();
    [[nodiscard]] bool try_acquire();
    void release();

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
    LockRank  rank_;
    LockMarks marks_;
};

}

#endif
