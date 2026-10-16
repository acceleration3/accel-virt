/* A ThreadLock held for the life of a scope: taken at construction, let go at
 * destruction, so no return path can leave it held. */
#ifndef AV_THREAD_LOCK_HOLD_HPP
#define AV_THREAD_LOCK_HOLD_HPP

namespace av {

class ThreadLock;

class ThreadLockHold {
public:
    explicit ThreadLockHold(ThreadLock &lock);
    ~ThreadLockHold();
    ThreadLockHold(const ThreadLockHold &) = delete;
    ThreadLockHold &operator=(const ThreadLockHold &) = delete;

private:
    ThreadLock &lock_;
};

}  // namespace av

#endif  // AV_THREAD_LOCK_HOLD_HPP
