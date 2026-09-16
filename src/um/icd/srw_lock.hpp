/* ThreadLock over a slim reader/writer lock taken exclusively: no kernel object
 * to create or close, so it is usable from the storage's first byte and needs
 * nothing at unload. Translation only. */
#ifndef AV_UM_SRW_LOCK_HPP
#define AV_UM_SRW_LOCK_HPP

#include "icd/common/thread_lock.hpp"

namespace av {

class SrwLock final : public icd::ThreadLock {
public:
    SrwLock();
    SrwLock(const SrwLock &) = delete;
    SrwLock &operator=(const SrwLock &) = delete;

    /* Each marks the calling thread's trace slot: blocked on, holding, given
     * back - how a hang names the thread that holds what another waits for. */
    void acquire() override;
    bool try_acquire() override;
    void release() override;

private:
    /* An SRWLOCK is one pointer, initialized to zero (SRWLOCK_INIT). */
    void *lock_;
};

}  // namespace av

#endif  // AV_UM_SRW_LOCK_HPP
