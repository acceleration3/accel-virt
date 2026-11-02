#ifndef AV_POSIX_LOCK_HPP
#define AV_POSIX_LOCK_HPP

#include "core/platform/thread_lock.hpp"

#include <pthread.h>

namespace av {

class PosixLock final : public ThreadLock {
public:
    PosixLock();
    ~PosixLock();
    PosixLock(const PosixLock &) = delete;
    PosixLock &operator=(const PosixLock &) = delete;

    void acquire() override;
    bool try_acquire() override;
    void release() override;

private:
    pthread_mutex_t mutex_;

    friend class PosixCondition;
};

}

#endif
