/* One attempt at a WaitMutex that does not wait, released when the scope ends if it
 * was taken: how a state dump reads what a lock guards without ever blocking
 * on a thread that holds it. */
#ifndef AV_WAIT_MUTEX_TRY_GUARD_HPP
#define AV_WAIT_MUTEX_TRY_GUARD_HPP

namespace av {

class WaitMutex;

class WaitMutexTryGuard {
public:
    explicit WaitMutexTryGuard(WaitMutex &lock);
    ~WaitMutexTryGuard();
    WaitMutexTryGuard(const WaitMutexTryGuard &) = delete;
    WaitMutexTryGuard &operator=(const WaitMutexTryGuard &) = delete;

    bool held() const;

private:
    WaitMutex &lock_;
    bool      held_;
};

}  // namespace av

#endif  // AV_WAIT_MUTEX_TRY_GUARD_HPP
