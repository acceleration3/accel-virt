#include "um/vulkan/process_heap.hpp"

#include <windows.h>

namespace av {

ProcessHeap::ProcessHeap() : heap_(GetProcessHeap()) {}

void *ProcessHeap::allocate(size_t bytes, Pool) { return HeapAlloc(heap_, HEAP_ZERO_MEMORY, bytes); }

void ProcessHeap::release(void *block) { static_cast<void>(HeapFree(heap_, 0u, block)); }

void ProcessHeap::require_irql_at_most(Irql, const char *) {}

}  // namespace av
