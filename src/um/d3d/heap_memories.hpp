#ifndef AV_UM_HEAP_MEMORIES_HPP
#define AV_UM_HEAP_MEMORIES_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "um/d3d/dedicated_heap_memory.hpp"
#include "um/d3d/lent_pages_heap_memory.hpp"
#include "um/d3d/stream_slice_heap_memory.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
class HostAllocation;
}

namespace av::d3d {
class D3dSession;
}

namespace av::d3d12 {

class HeapMemories {
public:
    HeapMemories(BlockSource &memory, d3d::D3dSession &session, DriverLog &lines);
    HeapMemories(const HeapMemories &) = delete;
    HeapMemories &operator=(const HeapMemories &) = delete;

    [[nodiscard]] Status take_slice(HostMemoryRequest::Kind kind, uint64_t bytes, HeapMemory *&made);
    [[nodiscard]] Status open_dedicated(HostMemoryRequest::Kind kind, uint64_t bytes, HeapMemory *&made);
    [[nodiscard]] Status place(uint64_t bytes, uint64_t named, HeapMemory *&made);
    [[nodiscard]] Status lend_pages(uint64_t bytes, HeapMemory *&made);
    [[nodiscard]] Status adopt(HostAllocation &allocation, HeapMemory *&made);

    bool invariants_hold() const;

private:
    d3d::D3dSession                  &session_;
    DriverLog                        &lines_;
    ObjectPool<StreamSliceHeapMemory> slices_;
    ObjectPool<DedicatedHeapMemory>   dedicated_;
    ObjectPool<LentPagesHeapMemory>   lent_;
};

}

#endif
