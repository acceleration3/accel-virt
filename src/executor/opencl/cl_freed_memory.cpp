#include "executor/opencl/cl_freed_memory.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClFreedMemory::ClFreedMemory() : total_(0u)
{
    AV_SCOPE("ClFreedMemory::ClFreedMemory");
}

void ClFreedMemory::add(uint64_t reach)
{
    AV_SCOPE("ClFreedMemory::add");
    uint64_t seen = __atomic_load_n(&total_, __ATOMIC_RELAXED);
    uint64_t next = 0u;
    do {
        next = reach > saturated - seen ? saturated : seen + reach;
    } while (!__atomic_compare_exchange_n(&total_, &seen, next, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
}

uint64_t ClFreedMemory::total() const
{
    AV_SCOPE("ClFreedMemory::total");
    return __atomic_load_n(&total_, __ATOMIC_ACQUIRE);
}

}
