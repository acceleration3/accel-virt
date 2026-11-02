#include "device/renderer/guest_memory_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint64_t offset_mask = GuestMemoryTable::max_region_size - 1u;

}

GuestMemoryTable::GuestMemoryTable() : regions_{}
{
    AV_SCOPE("GuestMemoryTable::GuestMemoryTable");
}

const GuestMemoryTable::Region *GuestMemoryTable::region_of(uint32_t region) const
{
    AV_SCOPE("GuestMemoryTable::region_of");
    for (const Region &entry : regions_)
        if (entry.used_ && entry.id_ == region) return &entry;
    return nullptr;
}

bool GuestMemoryTable::overlaps(uintptr_t base, uint64_t size) const
{
    AV_SCOPE("GuestMemoryTable::overlaps");
    for (const Region &entry : regions_)
        if (entry.used_ && base < entry.base_ + entry.size_ && entry.base_ < base + size) return true;
    return false;
}

bool GuestMemoryTable::well_formed(const Region &region) const
{
    AV_SCOPE("GuestMemoryTable::well_formed");
    return region.base_ != 0u && region.size_ != 0u && region.size_ <= max_region_size && region.id_ < region_limit &&
           region.descriptor_ >= 0 && region.size_ <= UINTPTR_MAX - region.base_;
}

Status GuestMemoryTable::add(uint32_t region, void *base, uint64_t size, int descriptor)
{
    AV_SCOPE("GuestMemoryTable::add");
    const Region candidate = {region, reinterpret_cast<uintptr_t>(base), size, descriptor, true};
    if (!well_formed(candidate)) return Status::invalid_parameter;
    if (region_of(region) != nullptr) return Status::conflict;
    if (overlaps(candidate.base_, size)) return Status::invalid_parameter;
    for (Region &entry : regions_) {
        if (entry.used_) continue;
        entry = candidate;
        return Status::ok;
    }
    return Status::no_space;
}

Status GuestMemoryTable::remove(uint32_t region, void *&base, uint64_t &size, int &descriptor)
{
    AV_SCOPE("GuestMemoryTable::remove");
    for (Region &entry : regions_) {
        if (!entry.used_ || entry.id_ != region) continue;
        base = reinterpret_cast<void *>(entry.base_);
        size = entry.size_;
        descriptor = entry.descriptor_;
        entry = Region{};
        return Status::ok;
    }
    return Status::not_found;
}

void *GuestMemoryTable::resolve(uint64_t encoded, uint64_t length) const
{
    AV_SCOPE("GuestMemoryTable::resolve");
    const Region *const region = region_of(static_cast<uint32_t>(encoded >> region_shift));
    const uint64_t      offset = encoded & offset_mask;
    if (region == nullptr || length == 0u || offset > region->size_ || length > region->size_ - offset) return nullptr;
    return reinterpret_cast<void *>(region->base_ + offset);
}

Status GuestMemoryTable::locate(const void *pointer, uint64_t length, int &descriptor, uint64_t &offset) const
{
    AV_SCOPE("GuestMemoryTable::locate");
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    if (length == 0u) return Status::not_found;
    for (const Region &entry : regions_) {
        if (!entry.used_ || address < entry.base_) continue;
        const uint64_t into = address - entry.base_;
        if (into >= entry.size_ || length > entry.size_ - into) continue;
        descriptor = entry.descriptor_;
        offset = into;
        return Status::ok;
    }
    return Status::not_found;
}

uint32_t GuestMemoryTable::count() const
{
    AV_SCOPE("GuestMemoryTable::count");
    uint32_t used = 0u;
    for (const Region &entry : regions_) used += entry.used_ ? 1u : 0u;
    return used;
}

bool GuestMemoryTable::invariants_hold() const
{
    AV_SCOPE("GuestMemoryTable::invariants_hold");
    for (uint32_t first = 0u; first < max_regions; first++) {
        const Region &region = regions_[first];
        if (!region.used_) continue;
        if (!well_formed(region)) return false;
        for (uint32_t second = first + 1u; second < max_regions; second++) {
            const Region &other = regions_[second];
            if (!other.used_) continue;
            if (other.id_ == region.id_ || (other.base_ < region.base_ + region.size_ && region.base_ < other.base_ + other.size_))
                return false;
        }
    }
    return true;
}

}
