/* A lock one thread holds while it uses something another thread may use too
 * (feature 045). An application calls into one driver from several threads,
 * and a driver's own state - its handles, a connection's staging and reply
 * slot - is shared between them. Not recursive: a thread that acquires twice
 * deadlocks, which the tests' recording lock turns into a failure. Each binary
 * implements it over its platform's lock (the guest drivers over an SRWLOCK). */
#ifndef AV_THREAD_LOCK_HPP
#define AV_THREAD_LOCK_HPP

namespace av {

class ThreadLock {
public:
    ThreadLock(const ThreadLock &) = delete;
    ThreadLock &operator=(const ThreadLock &) = delete;

    virtual void acquire() = 0;
    /* Takes the lock only if no thread holds it: a state dump must never wait
     * on a thread that may be the one hung (docs/trace.md). */
    virtual bool try_acquire() = 0;
    virtual void release() = 0;

protected:
    ThreadLock() = default;
    /* Non-virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~ThreadLock() = default;
};

}  // namespace av

#endif  // AV_THREAD_LOCK_HPP
