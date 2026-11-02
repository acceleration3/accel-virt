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

}

#endif
