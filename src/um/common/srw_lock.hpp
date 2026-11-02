#ifndef AV_UM_SRW_LOCK_HPP
#define AV_UM_SRW_LOCK_HPP

#include "core/platform/thread_lock.hpp"

namespace av {

class SrwLock final : public ThreadLock {
public:
    SrwLock();
    SrwLock(const SrwLock &) = delete;
    SrwLock &operator=(const SrwLock &) = delete;

    void acquire() override;
    bool try_acquire() override;
    void release() override;

private:
    void *lock_;
};

}

#endif
