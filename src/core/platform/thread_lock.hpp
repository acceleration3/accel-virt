#ifndef AV_THREAD_LOCK_HPP
#define AV_THREAD_LOCK_HPP

namespace av {

class ThreadLock {
public:
    ThreadLock(const ThreadLock &) = delete;
    ThreadLock &operator=(const ThreadLock &) = delete;

    virtual void acquire() = 0;
    virtual bool try_acquire() = 0;
    virtual void release() = 0;

protected:
    ThreadLock() = default;
    ~ThreadLock() = default;
};

}

#endif
