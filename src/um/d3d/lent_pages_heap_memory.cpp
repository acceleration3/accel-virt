#include "um/d3d/lent_pages_heap_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "um/d3d/session_memory.hpp"

namespace av::d3d12 {

LentPagesHeapMemory::LentPagesHeapMemory(ObjectPool<LentPagesHeapMemory> &home, d3d::SessionMemory &memory,
                                         HostAllocation &allocation, void *pages)
    : home_(home), memory_(memory), allocation_(allocation), pages_(pages)
{
    AV_SCOPE("LentPagesHeapMemory::LentPagesHeapMemory");
}

uint64_t LentPagesHeapMemory::handle() const
{
    AV_SCOPE("LentPagesHeapMemory::handle");
    return allocation_.handle();
}

uint64_t LentPagesHeapMemory::offset() const
{
    AV_SCOPE("LentPagesHeapMemory::offset");
    return 0u;
}

uint64_t LentPagesHeapMemory::window_bytes(uint64_t heap_bytes) const
{
    AV_SCOPE("LentPagesHeapMemory::window_bytes");
    return heap_bytes;
}

uint8_t *LentPagesHeapMemory::view(uint64_t heap_bytes) const
{
    AV_SCOPE("LentPagesHeapMemory::view");
    return allocation_.view_at(0u, heap_bytes);
}

void LentPagesHeapMemory::release()
{
    AV_SCOPE("LentPagesHeapMemory::release");
    memory_.retire_memory(allocation_);
    memory_.retire_pages(pages_);
    static_cast<void>(home_.destroy(this));
}

}
