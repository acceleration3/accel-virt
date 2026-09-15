/* One acquisition of a WaitMutex, released when the scope ends. */
#ifndef AV_WAIT_MUTEX_GUARD_HPP
#define AV_WAIT_MUTEX_GUARD_HPP

namespace av {

class WaitMutex;

class WaitMutexGuard {
public:
    explicit WaitMutexGuard(WaitMutex &mutex);
    ~WaitMutexGuard();
    WaitMutexGuard(const WaitMutexGuard &) = delete;
    WaitMutexGuard &operator=(const WaitMutexGuard &) = delete;

private:
    WaitMutex &mutex_;
};

}  // namespace av

#endif  // AV_WAIT_MUTEX_GUARD_HPP
