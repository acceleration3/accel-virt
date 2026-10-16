#include "executor/d3d/d3d12_address_map.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12AddressMap::D3d12AddressMap(BlockSource &blocks) : ranges_(blocks, Pool::paged)
{
    AV_SCOPE("D3d12AddressMap::D3d12AddressMap");
}

uint32_t D3d12AddressMap::below(uint64_t guest) const
{
    AV_SCOPE("D3d12AddressMap::below");
    uint32_t low = 0u, high = ranges_.size();
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (ranges_.at(middle).guest_ <= guest) low = middle + 1u;
        else high = middle;
    }
    return low == 0u ? ranges_.size() : low - 1u;
}

Status D3d12AddressMap::insert(uint64_t guest, uint64_t size, uint64_t host, uint32_t id)
{
    AV_SCOPE("D3d12AddressMap::insert");
    if (size == 0u || guest + size < guest || id == 0u) return Status::invalid_parameter;
    const uint32_t count = ranges_.size();
    const uint32_t before = below(guest);
    if (before != count && guest - ranges_.at(before).guest_ < ranges_.at(before).size_) return Status::conflict;
    const uint32_t at = before == count ? 0u : before + 1u;
    if (at < count && ranges_.at(at).guest_ - guest < size) return Status::conflict;
    return ranges_.insert_at(at, Range{guest, size, host, id});
}

void D3d12AddressMap::remove(uint32_t id)
{
    AV_SCOPE("D3d12AddressMap::remove");
    for (uint32_t index = 0u; index < ranges_.size(); index++) {
        if (ranges_.at(index).id_ != id) continue;
        ranges_.remove_at(index);
        return;
    }
}

uint64_t D3d12AddressMap::translate(uint64_t guest, uint64_t bytes) const
{
    AV_SCOPE("D3d12AddressMap::translate");
    uint64_t offset = 0u;
    const uint32_t id = owner(guest, offset);
    if (id == 0u) return 0u;
    const Range &range = ranges_.at(below(guest));
    if (bytes > range.size_ - offset) return 0u;
    return range.host_ + offset;
}

uint32_t D3d12AddressMap::owner(uint64_t guest, uint64_t &offset) const
{
    AV_SCOPE("D3d12AddressMap::owner");
    offset = 0u;
    const uint32_t at = below(guest);
    if (at == ranges_.size() || guest - ranges_.at(at).guest_ >= ranges_.at(at).size_) return 0u;
    offset = guest - ranges_.at(at).guest_;
    return ranges_.at(at).id_;
}

void D3d12AddressMap::clear()
{
    AV_SCOPE("D3d12AddressMap::clear");
    ranges_.clear();
}

uint32_t D3d12AddressMap::count() const
{
    AV_SCOPE("D3d12AddressMap::count");
    return ranges_.size();
}

bool D3d12AddressMap::invariants_hold() const
{
    AV_SCOPE("D3d12AddressMap::invariants_hold");
    if (!ranges_.invariants_hold()) return false;
    for (uint32_t index = 0u; index < ranges_.size(); index++) {
        const Range &range = ranges_.at(index);
        if (range.size_ == 0u || range.id_ == 0u) return false;
        if (index + 1u == ranges_.size()) continue;
        const Range &next = ranges_.at(index + 1u);
        if (next.guest_ <= range.guest_ || next.guest_ - range.guest_ < range.size_) return false;
    }
    return true;
}

}  // namespace av
