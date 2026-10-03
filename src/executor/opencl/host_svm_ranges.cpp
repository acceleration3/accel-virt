#include "executor/opencl/host_svm_ranges.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostSvmRanges::HostSvmRanges() : bases_(), sizes_(), owners_(), count_(0u)
{
    AV_SCOPE("HostSvmRanges::HostSvmRanges");
}

uint32_t HostSvmRanges::slot_of(uint64_t address) const
{
    AV_SCOPE("HostSvmRanges::slot_of");
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        if (bases_[slot] == address) return slot;
    }
    return count_;
}

bool HostSvmRanges::remember(void *base, uint64_t bytes, uint64_t owner)
{
    AV_SCOPE("HostSvmRanges::remember");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base));
    if (address == 0u || bytes == 0u) return false;
    /* A range that wraps could be resolved through either end of it. */
    if (bytes > ~uint64_t{0} - address) return false;
    const uint32_t held = slot_of(address);
    if (held != count_) {
        sizes_[held] = bytes;
        owners_[held] = owner;
        return true;
    }
    if (count_ == max_ranges) return false;
    bases_[count_] = address;
    sizes_[count_] = bytes;
    owners_[count_] = owner;
    ++count_;
    return true;
}

uint32_t HostSvmRanges::forget_owned(uint64_t owner)
{
    AV_SCOPE("HostSvmRanges::forget_owned");
    if (owner == 0u) return 0u;
    uint32_t forgotten = 0u;
    for (uint32_t slot = 0u; slot < count_;) {
        if (owners_[slot] != owner) {
            ++slot;
            continue;
        }
        static_cast<void>(forget(reinterpret_cast<void *>(static_cast<uintptr_t>(bases_[slot]))));
        ++forgotten;
    }
    return forgotten;
}

bool HostSvmRanges::forget(void *base)
{
    AV_SCOPE("HostSvmRanges::forget");
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base));
    if (address == 0u) return false;
    const uint32_t slot = slot_of(address);
    if (slot == count_) return false;
    --count_;
    bases_[slot] = bases_[count_];
    sizes_[slot] = sizes_[count_];
    owners_[slot] = owners_[count_];
    bases_[count_] = 0u;
    sizes_[count_] = 0u;
    owners_[count_] = 0u;
    return true;
}

void HostSvmRanges::forget_all()
{
    AV_SCOPE("HostSvmRanges::forget_all");
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        bases_[slot] = 0u;
        sizes_[slot] = 0u;
        owners_[slot] = 0u;
    }
    count_ = 0u;
}

void *HostSvmRanges::resolve(uint64_t address, uint64_t bytes) const
{
    AV_SCOPE("HostSvmRanges::resolve");
    if (address == 0u) return nullptr;
    if (bytes > ~uint64_t{0} - address) return nullptr;
    const uint64_t last = address + bytes;
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        const uint64_t end = bases_[slot] + sizes_[slot];
        /* The address itself has to be a byte of the range, even when nothing
         * is read from it: one past the end is not a place a command may name,
         * and it is also the first byte of whatever comes next. */
        if (address < bases_[slot] || address >= end) continue;
        if (last > end) continue;
        return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
    }
    return nullptr;
}

uint32_t HostSvmRanges::count() const
{
    AV_SCOPE("HostSvmRanges::count");
    return count_;
}

bool HostSvmRanges::invariants_hold() const
{
    AV_SCOPE("HostSvmRanges::invariants_hold");
    if (count_ > max_ranges) return false;
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        if (bases_[slot] == 0u || sizes_[slot] == 0u) return false;
        if (sizes_[slot] > ~uint64_t{0} - bases_[slot]) return false;
        for (uint32_t other = slot + 1u; other < count_; ++other) {
            if (bases_[other] == bases_[slot]) return false;
        }
    }
    for (uint32_t slot = count_; slot < max_ranges; ++slot) {
        if (bases_[slot] != 0u || sizes_[slot] != 0u || owners_[slot] != 0u) return false;
    }
    return true;
}

}  // namespace av
