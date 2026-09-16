/* A lock one application thread holds while it uses something another thread
 * may use too (feature 045). Vulkan lets an application call into one instance
 * from several threads, and the ICD's own state - its handles, a connection's
 * staging and reply slot - is shared between them. Not recursive: a thread that
 * acquires twice deadlocks, which the tests' recording lock turns into a
 * failure. The Windows shim implements it over an SRWLOCK. */
#ifndef AV_ICD_THREAD_LOCK_HPP
#define AV_ICD_THREAD_LOCK_HPP

namespace av::icd {

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

}  // namespace av::icd

#endif  // AV_ICD_THREAD_LOCK_HPP
