/* One attempt at a Mutex that does not wait, released when the scope ends if it
 * was taken: how a state dump reads what a lock guards without ever blocking
 * on a thread that holds it. */
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

}  // namespace av

#endif  // AV_MUTEX_TRY_GUARD_HPP
