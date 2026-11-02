#include "executor/shim/host_heap.hpp"

#include "core/trace/function_scope.hpp"

#include <stdlib.h>

namespace av {

HostHeap::HostHeap() : live_(0u)
{
    AV_SCOPE("HostHeap::HostHeap");
}

void *HostHeap::allocate(size_t bytes, Pool)
{
    AV_SCOPE("HostHeap::allocate");
    void *const block = calloc(1u, bytes);
    if (block != nullptr) __atomic_fetch_add(&live_, 1u, __ATOMIC_RELAXED);
    return block;
}

void HostHeap::release(void *block)
{
    AV_SCOPE("HostHeap::release");
    if (block == nullptr) return;
    __atomic_fetch_sub(&live_, 1u, __ATOMIC_RELAXED);
    free(block);
}

void HostHeap::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("HostHeap::require_irql_at_most");
}

uint64_t HostHeap::live() const
{
    AV_SCOPE("HostHeap::live");
    return __atomic_load_n(&live_, __ATOMIC_RELAXED);
}

}
