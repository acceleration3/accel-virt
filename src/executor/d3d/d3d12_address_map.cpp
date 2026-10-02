#include "executor/d3d/d3d12_address_map.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12AddressMap::D3d12AddressMap() : guest_{}, size_{}, host_{}, id_{}, count_(0u)
{
    AV_SCOPE("D3d12AddressMap::D3d12AddressMap");
}

uint32_t D3d12AddressMap::below(uint64_t guest) const
{
    AV_SCOPE("D3d12AddressMap::below");
    uint32_t low = 0u, high = count_;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (guest_[middle] <= guest) low = middle + 1u;
        else high = middle;
    }
    return low == 0u ? count_ : low - 1u;
}

bool D3d12AddressMap::insert(uint64_t guest, uint64_t size, uint64_t host, uint32_t id)
{
    AV_SCOPE("D3d12AddressMap::insert");
    if (size == 0u || guest + size < guest || count_ == capacity || id == 0u) return false;
    const uint32_t before = below(guest);
    if (before != count_ && guest - guest_[before] < size_[before]) return false;
    const uint32_t at = before == count_ ? 0u : before + 1u;
    if (at < count_ && guest_[at] - guest < size) return false;
    for (uint32_t index = count_; index > at; index--) {
        guest_[index] = guest_[index - 1u];
        size_[index] = size_[index - 1u];
        host_[index] = host_[index - 1u];
        id_[index] = id_[index - 1u];
    }
    guest_[at] = guest;
    size_[at] = size;
    host_[at] = host;
    id_[at] = id;
    count_++;
    return true;
}

void D3d12AddressMap::remove(uint32_t id)
{
    AV_SCOPE("D3d12AddressMap::remove");
    for (uint32_t index = 0u; index < count_; index++) {
        if (id_[index] != id) continue;
        for (uint32_t next = index; next + 1u < count_; next++) {
            guest_[next] = guest_[next + 1u];
            size_[next] = size_[next + 1u];
            host_[next] = host_[next + 1u];
            id_[next] = id_[next + 1u];
        }
        count_--;
        return;
    }
}

uint64_t D3d12AddressMap::translate(uint64_t guest, uint64_t bytes) const
{
    AV_SCOPE("D3d12AddressMap::translate");
    uint64_t offset = 0u;
    const uint32_t id = owner(guest, offset);
    if (id == 0u) return 0u;
    const uint32_t at = below(guest);
    if (bytes > size_[at] - offset) return 0u;
    return host_[at] + offset;
}

uint32_t D3d12AddressMap::owner(uint64_t guest, uint64_t &offset) const
{
    AV_SCOPE("D3d12AddressMap::owner");
    offset = 0u;
    const uint32_t at = below(guest);
    if (at == count_ || guest - guest_[at] >= size_[at]) return 0u;
    offset = guest - guest_[at];
    return id_[at];
}

void D3d12AddressMap::clear()
{
    AV_SCOPE("D3d12AddressMap::clear");
    count_ = 0u;
}

uint32_t D3d12AddressMap::count() const
{
    AV_SCOPE("D3d12AddressMap::count");
    return count_;
}

bool D3d12AddressMap::invariants_hold() const
{
    AV_SCOPE("D3d12AddressMap::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t index = 0u; index < count_; index++) {
        if (size_[index] == 0u || id_[index] == 0u) return false;
        if (index + 1u < count_ && guest_[index + 1u] - guest_[index] < size_[index]) return false;
        if (index + 1u < count_ && guest_[index + 1u] <= guest_[index]) return false;
    }
    return true;
}

}  // namespace av
