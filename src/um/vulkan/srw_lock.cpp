#include "um/vulkan/srw_lock.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_table.hpp"

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
    TraceTable *const table = scope_binding.table();
    const uint64_t    address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
    if (!TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_))) {
        if (table != nullptr) table->waiting(address);
        AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
    }
    if (table != nullptr) table->acquired(address);
}

bool SrwLock::try_acquire()
{
    AV_SCOPE("SrwLock::try_acquire");
    if (!TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_))) return false;
    TraceTable *const table = scope_binding.table();
    if (table != nullptr) table->acquired(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));
    return true;
}

void SrwLock::release()
{
    AV_SCOPE("SrwLock::release");
    TraceTable *const table = scope_binding.table();
    if (table != nullptr) table->released(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));
    ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

}  // namespace av
