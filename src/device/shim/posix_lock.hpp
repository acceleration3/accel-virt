/* ThreadLock over a pthread mutex: the one lock of the host binaries - the
 * device module, av-renderer and the executor library - as SrwLock is the
 * guest drivers'. Each step marks the calling thread's trace slot (LockMarks):
 * blocked on it, holding it, given back - so a hang reads in avinspector's
 * stalls and threads as which thread holds what another waits for. Not
 * recursive. Translation only. */
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

    /* A condition waits on the mutex itself, and marks its own steps. */
    friend class PosixCondition;
};

}  // namespace av

#endif  // AV_POSIX_LOCK_HPP
