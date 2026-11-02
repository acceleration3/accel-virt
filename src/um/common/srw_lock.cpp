#include "um/common/srw_lock.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"

#include <windows.h>

static_assert(sizeof(SRWLOCK) == sizeof(void *));

namespace av {

SrwLock::SrwLock() : lock_(nullptr)
{
    AV_SCOPE("SrwLock::SrwLock");
}

void SrwLock::acquire()
{
    AV_SCOPE("SrwLock::acquire");
    const LockMarks marks(this);
    if (!TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_))) {
        marks.waiting();
        AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
    }
    marks.acquired();
}

bool SrwLock::try_acquire()
{
    AV_SCOPE("SrwLock::try_acquire");
    if (!TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_))) return false;
    LockMarks(this).acquired();
    return true;
}

void SrwLock::release()
{
    AV_SCOPE("SrwLock::release");
    LockMarks(this).released();
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

}
