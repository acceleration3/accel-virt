#include "executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"

namespace av {

GuestRegion::GuestRegion() : memory_(nullptr), base_(nullptr), length_(0u)
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

bool GuestRegion::adopt(ImportedBlob &blob)
{
    AV_SCOPE("GuestRegion::adopt");
    const SourceMemory &memory = blob.memory();
    if (memory_ != nullptr || memory.pointer() == nullptr || memory.size() == 0u) return false;
    memory_ = &memory;
    base_ = memory.pointer();
    length_ = memory.size();
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base_));
    blob.record(address, address);
    return true;
}

void GuestRegion::abandon(ImportedBlob &blob)
{
    AV_SCOPE("GuestRegion::abandon");
    if (memory_ == nullptr || &blob.memory() != memory_) return;
    memory_ = nullptr;
    base_ = nullptr;
    length_ = 0u;
    blob.record(0u, 0u);
}

const SourceMemory *GuestRegion::memory() const
{
    AV_SCOPE("GuestRegion::memory");
    return memory_;
}

uint8_t *GuestRegion::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("GuestRegion::at");
    if (base_ == nullptr) return nullptr;
    if (offset > length_) return nullptr;
    /* Subtraction rather than `offset + bytes`, which is what a guest would
     * pick the numbers to wrap. */
    if (bytes > length_ - offset) return nullptr;
    return base_ + offset;
}

bool GuestRegion::holds(const void *bytes) const
{
    AV_SCOPE("GuestRegion::holds");
    if (base_ == nullptr) return false;
    /* As integers: the pointer may be into any other object. One below the
     * region wraps to an offset past it. */
    return reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(base_) < length_;
}

bool GuestRegion::publish(uint64_t offset, uint64_t value)
{
    AV_SCOPE("GuestRegion::publish");
    uint8_t *const word = at(offset, sizeof(uint64_t));
    if (word == nullptr || reinterpret_cast<uintptr_t>(word) % sizeof(uint64_t) != 0u) return false;
    __atomic_store_n(reinterpret_cast<uint64_t *>(word), value, __ATOMIC_RELEASE);
    return true;
}

uint64_t GuestRegion::length() const
{
    AV_SCOPE("GuestRegion::length");
    return length_;
}

bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    return (memory_ != nullptr) == (base_ != nullptr) && (base_ != nullptr || length_ == 0u);
}

}  // namespace av
