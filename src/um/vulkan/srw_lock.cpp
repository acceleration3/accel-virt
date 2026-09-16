#include "um/vulkan/srw_lock.hpp"

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
    AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

void SrwLock::release()
{
    AV_SCOPE("SrwLock::release");
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

}  // namespace av
