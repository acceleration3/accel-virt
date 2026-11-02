#ifndef AV_MUTEX_TRY_GUARD_HPP
#define AV_MUTEX_TRY_GUARD_HPP

namespace av {

class Mutex;

class MutexTryGuard {
public:
    explicit MutexTryGuard(Mutex &lock);
    ~MutexTryGuard();
    MutexTryGuard(const MutexTryGuard &) = delete;
    MutexTryGuard &operator=(const MutexTryGuard &) = delete;

    bool held() const;

private:
    Mutex &lock_;
    bool  held_;
};

}

#endif
