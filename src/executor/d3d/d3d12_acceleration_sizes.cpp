#include "executor/d3d/d3d12_acceleration_sizes.hpp"

#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

D3d12AccelerationSizes::D3d12AccelerationSizes(BlockSource &blocks) : entries_(blocks, Pool::paged)
{
    AV_SCOPE("D3d12AccelerationSizes::D3d12AccelerationSizes");
}

bool D3d12AccelerationSizes::note(uint64_t host, uint64_t bytes)
{
    AV_SCOPE("D3d12AccelerationSizes::note");
    if (host == 0u || bytes == 0u) return false;
    const SortedSearch<Entry, &Entry::host_> search(entries_);
    const uint32_t                            at = search.at_or_after(host);
    if (at < entries_.size() && entries_.at(at).host_ == host) {
        entries_.at(at).bytes_ = bytes;
        return true;
    }
    if (entries_.size() >= most_structures) return false;
    return entries_.insert_at(at, Entry{host, bytes}) == Status::ok;
}

bool D3d12AccelerationSizes::find(uint64_t host, uint64_t &bytes) const
{
    AV_SCOPE("D3d12AccelerationSizes::find");
    const SortedSearch<Entry, &Entry::host_> search(entries_);
    const uint32_t                            at = search.at_or_after(host);
    if (at >= entries_.size() || entries_.at(at).host_ != host) return false;
    bytes = entries_.at(at).bytes_;
    return true;
}

uint32_t D3d12AccelerationSizes::count() const
{
    AV_SCOPE("D3d12AccelerationSizes::count");
    return entries_.size();
}

void D3d12AccelerationSizes::clear()
{
    AV_SCOPE("D3d12AccelerationSizes::clear");
    entries_.clear();
}

bool D3d12AccelerationSizes::invariants_hold() const
{
    AV_SCOPE("D3d12AccelerationSizes::invariants_hold");
    for (uint32_t index = 1u; index < entries_.size(); index++)
        if (entries_.at(index - 1u).host_ >= entries_.at(index).host_) return false;
    return true;
}

}
