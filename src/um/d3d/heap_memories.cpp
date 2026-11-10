#include "um/d3d/heap_memories.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "um/d3d/d3d_session.hpp"

#include <windows.h>

namespace av::d3d12 {

namespace {
constexpr uint64_t page_bytes = 4096u;
}

HeapMemories::HeapMemories(BlockSource &memory, d3d::D3dSession &session, DriverLog &lines)
    : session_(session), lines_(lines), slices_(memory), dedicated_(memory), lent_(memory)
{
    AV_SCOPE("HeapMemories::HeapMemories");
}

Status HeapMemories::take_slice(HostMemoryRequest::Kind kind, uint64_t bytes, HeapMemory *&made)
{
    AV_SCOPE("HeapMemories::take_slice");
    made = nullptr;
    icd::StreamMemory *const stream = session_.shared_memory().stream(kind);
    if (stream == nullptr) return Status::invalid_state;
    icd::StreamMemory::Slice slice = {};
    const Status             taken = stream->take_slice(bytes, slice);
    if (taken != Status::ok) return taken;
    StreamSliceHeapMemory *kept = nullptr;
    const Status           created = slices_.create(kept, slices_, *stream, slice);
    if (created != Status::ok) {
        stream->give_back(slice);
        return created;
    }
    made = kept;
    return Status::ok;
}

Status HeapMemories::open_dedicated(HostMemoryRequest::Kind kind, uint64_t bytes, HeapMemory *&made)
{
    AV_SCOPE("HeapMemories::open_dedicated");
    made = nullptr;
    HostAllocation *allocation = nullptr;
    const Status    opened = session_.open_memory(bytes, HostMemoryRequest(kind, 0u), allocation);
    if (opened != Status::ok) return opened;
    DedicatedHeapMemory *kept = nullptr;
    const Status         created = dedicated_.create(kept, dedicated_, session_.shared_memory(), *allocation);
    if (created != Status::ok) {
        session_.close_memory(*allocation);
        return created;
    }
    made = kept;
    return Status::ok;
}

Status HeapMemories::place(uint64_t bytes, uint64_t named, HeapMemory *&made)
{
    AV_SCOPE("HeapMemories::place");
    made = nullptr;
    HostAllocation *allocation = nullptr;
    const Status    opened =
        session_.place_memory(bytes, HostMemoryRequest(HostMemoryRequest::Kind::placed_heap, 0u), named, allocation);
    if (opened != Status::ok) return opened;
    DedicatedHeapMemory *kept = nullptr;
    const Status         created = dedicated_.create(kept, dedicated_, session_.shared_memory(), *allocation);
    if (created != Status::ok) {
        session_.close_memory(*allocation);
        return created;
    }
    made = kept;
    return Status::ok;
}

Status HeapMemories::adopt(HostAllocation &allocation, HeapMemory *&made)
{
    AV_SCOPE("HeapMemories::adopt");
    made = nullptr;
    DedicatedHeapMemory *kept = nullptr;
    const Status         created = dedicated_.create(kept, dedicated_, session_.shared_memory(), allocation);
    if (created != Status::ok) return created;
    made = kept;
    return Status::ok;
}

Status HeapMemories::lend_pages(uint64_t bytes, HeapMemory *&made)
{
    AV_SCOPE("HeapMemories::lend_pages");
    made = nullptr;
    const uint64_t pages = (bytes + page_bytes - 1u) & ~(page_bytes - 1u);
    void *const    own = VirtualAlloc(nullptr, static_cast<SIZE_T>(pages), MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH,
                                      PAGE_READWRITE);
    if (own == nullptr) return Status::no_memory;
    HostAllocation *allocation = nullptr;
    const Status    lent =
        session_.adopt_memory(own, pages, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), allocation);
    if (lent != Status::ok) {
        static_cast<void>(VirtualFree(own, 0u, MEM_RELEASE));
        lines_.write("the device took no write-back heap's pages; its memory is the host's");
        return lent;
    }
    LentPagesHeapMemory *kept = nullptr;
    const Status         created = lent_.create(kept, lent_, session_.shared_memory(), *allocation, own);
    if (created != Status::ok) {
        session_.close_memory(*allocation);
        static_cast<void>(VirtualFree(own, 0u, MEM_RELEASE));
        return created;
    }
    made = kept;
    return Status::ok;
}

bool HeapMemories::invariants_hold() const
{
    AV_SCOPE("HeapMemories::invariants_hold");
    return slices_.invariants_hold() && dedicated_.invariants_hold() && lent_.invariants_hold();
}

}
