#include "icd/d3d/d3d12_heap_arena.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/thread_lock_hold.hpp"

namespace av::d3d {

D3d12HeapArena::D3d12HeapArena(icd::ThreadLock &lock) : lock_(lock), heaps_(alignment)
{
    AV_SCOPE("D3d12HeapArena::D3d12HeapArena");
}

void D3d12HeapArena::take(uint64_t begin, uint64_t end)
{
    AV_SCOPE("D3d12HeapArena::take");
    const icd::ThreadLockHold release(lock_);
    heaps_.reset(begin, end);
}

Status D3d12HeapArena::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("D3d12HeapArena::place");
    offset = 0u;
    const icd::ThreadLockHold release(lock_);
    return heaps_.place(bytes, offset);
}

Status D3d12HeapArena::remove(uint64_t offset)
{
    AV_SCOPE("D3d12HeapArena::remove");
    const icd::ThreadLockHold release(lock_);
    return heaps_.remove(offset);
}

uint32_t D3d12HeapArena::placed() const
{
    AV_SCOPE("D3d12HeapArena::placed");
    return heaps_.placed();
}

uint64_t D3d12HeapArena::bytes_placed() const
{
    AV_SCOPE("D3d12HeapArena::bytes_placed");
    return heaps_.bytes_placed();
}

bool D3d12HeapArena::invariants_hold() const
{
    AV_SCOPE("D3d12HeapArena::invariants_hold");
    return heaps_.invariants_hold();
}

}  // namespace av::d3d
