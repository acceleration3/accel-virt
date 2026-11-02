#include "device/shim/posix_condition.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"
#include "device/shim/posix_lock.hpp"

namespace av {

PosixCondition::PosixCondition(PosixLock &lock) : lock_(lock), condition_()
{
    AV_SCOPE("PosixCondition::PosixCondition");
    pthread_condattr_t attributes;
    static_cast<void>(pthread_condattr_init(&attributes));
    static_cast<void>(pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC));
    static_cast<void>(pthread_cond_init(&condition_, &attributes));
    static_cast<void>(pthread_condattr_destroy(&attributes));
}

PosixCondition::~PosixCondition()
{
    AV_SCOPE("PosixCondition::~PosixCondition");
    static_cast<void>(pthread_cond_destroy(&condition_));
}

void PosixCondition::wait()
{
    AV_SCOPE("PosixCondition::wait");
    const LockMarks held(&lock_);
    held.released();
    LockMarks(this).waiting();
    static_cast<void>(pthread_cond_wait(&condition_, &lock_.mutex_));
    held.acquired();
}

bool PosixCondition::wait_until(const timespec &deadline)
{
    AV_SCOPE("PosixCondition::wait_until");
    const LockMarks held(&lock_);
    held.released();
    LockMarks(this).waiting();
    const int waited = pthread_cond_timedwait(&condition_, &lock_.mutex_, &deadline);
    held.acquired();
    return waited == 0;
}

void PosixCondition::signal()
{
    AV_SCOPE("PosixCondition::signal");
    static_cast<void>(pthread_cond_signal(&condition_));
}

void PosixCondition::broadcast()
{
    AV_SCOPE("PosixCondition::broadcast");
    static_cast<void>(pthread_cond_broadcast(&condition_));
}

}
