#include "um/d3d/dedicated_heap_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "um/d3d/session_memory.hpp"

namespace av::d3d12 {

DedicatedHeapMemory::DedicatedHeapMemory(ObjectPool<DedicatedHeapMemory> &home, d3d::SessionMemory &memory,
                                         HostAllocation &allocation)
    : home_(home), memory_(memory), allocation_(allocation)
{
    AV_SCOPE("DedicatedHeapMemory::DedicatedHeapMemory");
}

uint64_t DedicatedHeapMemory::handle() const
{
    AV_SCOPE("DedicatedHeapMemory::handle");
    return allocation_.handle();
}

uint64_t DedicatedHeapMemory::offset() const
{
    AV_SCOPE("DedicatedHeapMemory::offset");
    return 0u;
}

uint64_t DedicatedHeapMemory::window_bytes(uint64_t heap_bytes) const
{
    AV_SCOPE("DedicatedHeapMemory::window_bytes");
    return heap_bytes;
}

uint8_t *DedicatedHeapMemory::view(uint64_t heap_bytes) const
{
    AV_SCOPE("DedicatedHeapMemory::view");
    return allocation_.view_at(0u, heap_bytes);
}

void DedicatedHeapMemory::release()
{
    AV_SCOPE("DedicatedHeapMemory::release");
    memory_.retire_memory(allocation_);
    static_cast<void>(home_.destroy(this));
}

}
