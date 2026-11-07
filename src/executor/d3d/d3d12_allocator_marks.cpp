#include "executor/d3d/d3d12_allocator_marks.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12AllocatorMarks::D3d12AllocatorMarks(BlockSource &blocks) : allocators_(blocks, Pool::paged), marks_(blocks, Pool::paged)
{
    AV_SCOPE("D3d12AllocatorMarks::D3d12AllocatorMarks");
}

Status D3d12AllocatorMarks::bind(uint32_t list, uint32_t allocator)
{
    AV_SCOPE("D3d12AllocatorMarks::bind");
    uint32_t *const found = allocators_.find(list);
    if (found != nullptr) {
        *found = allocator;
        return Status::ok;
    }
    uint32_t slot = 0u;
    return allocators_.insert(list, allocator, slot);
}

Status D3d12AllocatorMarks::submitted(uint32_t list, uint64_t mark)
{
    AV_SCOPE("D3d12AllocatorMarks::submitted");
    const uint32_t *const allocator = allocators_.find(list);
    if (allocator == nullptr) return Status::ok;
    uint64_t *const found = marks_.find(*allocator);
    if (found != nullptr) {
        if (mark > *found) *found = mark;
        return Status::ok;
    }
    uint32_t slot = 0u;
    return marks_.insert(*allocator, mark, slot);
}

void D3d12AllocatorMarks::forget_list(uint32_t list)
{
    AV_SCOPE("D3d12AllocatorMarks::forget_list");
    static_cast<void>(allocators_.erase(list));
}

void D3d12AllocatorMarks::forget_allocator(uint32_t allocator)
{
    AV_SCOPE("D3d12AllocatorMarks::forget_allocator");
    static_cast<void>(marks_.erase(allocator));
    allocators_.erase_if([allocator](uint32_t, uint32_t &bound) { return bound == allocator; });
}

void D3d12AllocatorMarks::clear()
{
    AV_SCOPE("D3d12AllocatorMarks::clear");
    allocators_.clear();
    marks_.clear();
}

uint64_t D3d12AllocatorMarks::mark_of(uint32_t allocator) const
{
    AV_SCOPE("D3d12AllocatorMarks::mark_of");
    const uint64_t *const found = marks_.find(allocator);
    return found != nullptr ? *found : 0u;
}

uint32_t D3d12AllocatorMarks::lists() const
{
    AV_SCOPE("D3d12AllocatorMarks::lists");
    return allocators_.count();
}

bool D3d12AllocatorMarks::invariants_hold() const
{
    AV_SCOPE("D3d12AllocatorMarks::invariants_hold");
    return allocators_.invariants_hold() && marks_.invariants_hold();
}

}
