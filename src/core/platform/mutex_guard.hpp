/* One acquisition of a Mutex, released when the scope ends. */
#ifndef AV_MUTEX_GUARD_HPP
#define AV_MUTEX_GUARD_HPP

namespace av {

class Mutex;

class MutexGuard {
public:
    explicit MutexGuard(Mutex &mutex);
    ~MutexGuard();
    MutexGuard(const MutexGuard &) = delete;
    MutexGuard &operator=(const MutexGuard &) = delete;

private:
    Mutex &mutex_;
};

}  // namespace av

#endif  // AV_MUTEX_GUARD_HPP
