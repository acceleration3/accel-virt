#include "um/vulkan/srw_lock.hpp"

#include <windows.h>

static_assert(sizeof(SRWLOCK) == sizeof(void *));

namespace av {

SrwLock::SrwLock() : lock_(nullptr) {}

void SrwLock::acquire() { AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_)); }

void SrwLock::release() { ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_)); }

}  // namespace av
