#include "icd/d3d/d3d12_heap_arena.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::d3d {

D3d12HeapArena::D3d12HeapArena(icd::ThreadLock &lock)
    : lock_(lock), begin_(0u), end_(0u), offsets_{}, sizes_{}, count_(0u)
{
    AV_SCOPE("D3d12HeapArena::D3d12HeapArena");
}

uint64_t D3d12HeapArena::round_up(uint64_t value) const
{
    AV_SCOPE("D3d12HeapArena::round_up");
    return (value + (alignment - 1u)) & ~(alignment - 1u);
}

void D3d12HeapArena::take(uint64_t begin, uint64_t end)
{
    AV_SCOPE("D3d12HeapArena::take");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    begin_ = round_up(begin);
    end_ = end & ~(alignment - 1u);
    if (end_ < begin_) end_ = begin_;
    count_ = 0u;
}

bool D3d12HeapArena::gap(uint64_t bytes, uint64_t &offset, uint32_t &index) const
{
    AV_SCOPE("D3d12HeapArena::gap");
    uint64_t at = begin_;
    for (index = 0u; index < count_; index++) {
        if (offsets_[index] - at >= bytes) break;
        at = offsets_[index] + sizes_[index];
    }
    if (end_ - at < bytes) return false;
    offset = at;
    return true;
}

Status D3d12HeapArena::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("D3d12HeapArena::place");
    offset = 0u;
    if (bytes == 0u) return Status::invalid_parameter;
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (count_ == max_heaps || bytes > end_ - begin_) return Status::no_space;
    const uint64_t rounded = round_up(bytes);
    uint64_t at = 0u;
    uint32_t index = 0u;
    if (!gap(rounded, at, index)) return Status::no_space;
    for (uint32_t moved = count_; moved > index; moved--) {
        offsets_[moved] = offsets_[moved - 1u];
        sizes_[moved] = sizes_[moved - 1u];
    }
    offsets_[index] = at;
    sizes_[index] = rounded;
    count_++;
    offset = at;
    return Status::ok;
}

Status D3d12HeapArena::remove(uint64_t offset)
{
    AV_SCOPE("D3d12HeapArena::remove");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    uint32_t index = 0u;
    while (index < count_ && offsets_[index] != offset) index++;
    if (index == count_) return Status::invalid_parameter;
    for (; index + 1u < count_; index++) {
        offsets_[index] = offsets_[index + 1u];
        sizes_[index] = sizes_[index + 1u];
    }
    count_--;
    return Status::ok;
}

uint32_t D3d12HeapArena::placed() const
{
    AV_SCOPE("D3d12HeapArena::placed");
    return count_;
}

uint64_t D3d12HeapArena::bytes_placed() const
{
    AV_SCOPE("D3d12HeapArena::bytes_placed");
    uint64_t total = 0u;
    for (uint32_t index = 0u; index < count_; index++) total += sizes_[index];
    return total;
}

bool D3d12HeapArena::invariants_hold() const
{
    AV_SCOPE("D3d12HeapArena::invariants_hold");
    if (begin_ % alignment != 0u || end_ % alignment != 0u || end_ < begin_ || count_ > max_heaps) return false;
    uint64_t at = begin_;
    for (uint32_t index = 0u; index < count_; index++) {
        if (offsets_[index] < at || offsets_[index] % alignment != 0u || sizes_[index] == 0u) return false;
        if (sizes_[index] % alignment != 0u || sizes_[index] > end_ - offsets_[index]) return false;
        at = offsets_[index] + sizes_[index];
    }
    return true;
}

}  // namespace av::d3d
