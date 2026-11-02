#include "executor/opencl/host_svm_ranges.hpp"

#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

HostSvmRanges::HostSvmRanges(BlockSource &blocks) : ranges_(blocks, Pool::paged), widest_(0u)
{
    AV_SCOPE("HostSvmRanges::HostSvmRanges");
}

uint32_t HostSvmRanges::above(uint64_t address) const
{
    AV_SCOPE("HostSvmRanges::above");
    return SortedSearch<Range, &Range::base_>(ranges_).above(address);
}

Status HostSvmRanges::make_room()
{
    AV_SCOPE("HostSvmRanges::make_room");
    return ranges_.reserve(ranges_.size() + 1u);
}

Status HostSvmRanges::remember(void *base, uint64_t bytes, uint64_t owner)
{
    AV_SCOPE("HostSvmRanges::remember");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base));
    if (address == 0u || bytes == 0u) return Status::invalid_parameter;
    if (bytes > ~uint64_t{0} - address) return Status::invalid_parameter;
    const uint32_t after = above(address);
    if (after != 0u && ranges_.at(after - 1u).base_ == address) {
        ranges_.at(after - 1u) = Range{address, bytes, owner};
    } else {
        const Status inserted = ranges_.insert_at(after, Range{address, bytes, owner});
        if (inserted != Status::ok) return inserted;
    }
    if (bytes > widest_) widest_ = bytes;
    return Status::ok;
}

uint32_t HostSvmRanges::forget_owned(uint64_t owner)
{
    AV_SCOPE("HostSvmRanges::forget_owned");
    if (owner == 0u) return 0u;
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < ranges_.size(); index++)
        if (ranges_.at(index).owner_ != owner) ranges_.at(kept++) = ranges_.at(index);
    const uint32_t forgotten = ranges_.size() - kept;
    ranges_.truncate(kept);
    if (kept == 0u) widest_ = 0u;
    return forgotten;
}

bool HostSvmRanges::forget(void *base)
{
    AV_SCOPE("HostSvmRanges::forget");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base));
    if (address == 0u) return false;
    const uint32_t after = above(address);
    if (after == 0u || ranges_.at(after - 1u).base_ != address) return false;
    ranges_.remove_at(after - 1u);
    if (ranges_.size() == 0u) widest_ = 0u;
    return true;
}

void HostSvmRanges::forget_all()
{
    AV_SCOPE("HostSvmRanges::forget_all");
    ranges_.clear();
    widest_ = 0u;
}

void *HostSvmRanges::resolve(uint64_t address, uint64_t bytes) const
{
    AV_SCOPE("HostSvmRanges::resolve");
    if (address == 0u) return nullptr;
    if (bytes > ~uint64_t{0} - address) return nullptr;
    const uint64_t last = address + bytes;
    for (uint32_t index = above(address); index != 0u; index--) {
        const Range &range = ranges_.at(index - 1u);
        if (address - range.base_ >= widest_) break;
        if (address - range.base_ < range.size_ && last - range.base_ <= range.size_)
            return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
    }
    return nullptr;
}

uint32_t HostSvmRanges::count() const
{
    AV_SCOPE("HostSvmRanges::count");
    return ranges_.size();
}

bool HostSvmRanges::invariants_hold() const
{
    AV_SCOPE("HostSvmRanges::invariants_hold");
    if (!ranges_.invariants_hold()) return false;
    uint64_t below = 0u;
    for (const Range &range : ranges_) {
        if (range.base_ <= below || range.size_ == 0u || range.size_ > widest_) return false;
        if (range.size_ > ~uint64_t{0} - range.base_) return false;
        below = range.base_;
    }
    return true;
}

}
