#include "device/shim/posix_lock.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PosixLock::PosixLock() : mutex_(PTHREAD_MUTEX_INITIALIZER)
{
    AV_SCOPE("PosixLock::PosixLock");
}

PosixLock::~PosixLock()
{
    AV_SCOPE("PosixLock::~PosixLock");
    static_cast<void>(pthread_mutex_destroy(&mutex_));
}

/* Marked waiting only when the mutex is taken already: an uncontended
 * acquire is no wait worth showing. */
void PosixLock::acquire()
{
    AV_SCOPE("PosixLock::acquire");
    const LockMarks marks(this);
    if (pthread_mutex_trylock(&mutex_) != 0) {
        marks.waiting();
        static_cast<void>(pthread_mutex_lock(&mutex_));
    }
    marks.acquired();
}

bool PosixLock::try_acquire()
{
    AV_SCOPE("PosixLock::try_acquire");
    if (pthread_mutex_trylock(&mutex_) != 0) return false;
    LockMarks(this).acquired();
    return true;
}

void PosixLock::release()
{
    AV_SCOPE("PosixLock::release");
    LockMarks(this).released();
    static_cast<void>(pthread_mutex_unlock(&mutex_));
}

}  // namespace av
