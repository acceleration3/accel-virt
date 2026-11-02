#include "um/common/process_heap.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

ProcessHeap::ProcessHeap() : heap_(GetProcessHeap())
{
    AV_SCOPE("ProcessHeap::ProcessHeap");
}

void *ProcessHeap::allocate(size_t bytes, Pool)
{
    AV_SCOPE("ProcessHeap::allocate");
    return HeapAlloc(heap_, HEAP_ZERO_MEMORY, bytes);
}

void ProcessHeap::release(void *block)
{
    AV_SCOPE("ProcessHeap::release");
    static_cast<void>(HeapFree(heap_, 0u, block));
}

void ProcessHeap::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("ProcessHeap::require_irql_at_most");
}

}
