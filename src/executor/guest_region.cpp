#include "executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/source_memory.hpp"

namespace av {

GuestRegion::GuestRegion() : memory_(nullptr)
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

bool GuestRegion::adopt(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("GuestRegion::adopt");
    handle = 0u;
    if (memory_ != nullptr || memory.pointer() == nullptr || memory.size() == 0u) return false;
    memory_ = &memory;
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory.pointer()));
    return true;
}

void GuestRegion::abandon(const SourceMemory &memory)
{
    AV_SCOPE("GuestRegion::abandon");
    if (is(memory)) memory_ = nullptr;
}

bool GuestRegion::is(const SourceMemory &memory) const
{
    AV_SCOPE("GuestRegion::is");
    return memory_ == &memory;
}

const SourceMemory *GuestRegion::memory() const
{
    AV_SCOPE("GuestRegion::memory");
    return memory_;
}

uint8_t *GuestRegion::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("GuestRegion::at");
    return memory_ != nullptr ? memory_->at(offset, bytes) : nullptr;
}

bool GuestRegion::holds(const void *bytes) const
{
    AV_SCOPE("GuestRegion::holds");
    if (memory_ == nullptr) return false;
    /* As integers: the pointer may be into any other object. One below the
     * region wraps to an offset past it. */
    return reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(memory_->pointer()) < memory_->size();
}

bool GuestRegion::publish(uint64_t offset, uint64_t value)
{
    AV_SCOPE("GuestRegion::publish");
    uint8_t *const word = at(offset, sizeof(uint64_t));
    if (word == nullptr || reinterpret_cast<uintptr_t>(word) % sizeof(uint64_t) != 0u) return false;
    __atomic_store_n(reinterpret_cast<uint64_t *>(word), value, __ATOMIC_RELEASE);
    /* The region is the memory source's mapping, which a driver may have made
     * write-combined (docs/host-memory.md): x86 orders such a store with the
     * device's own - the batch head the guest reads before the word - only
     * at a store fence. */
    __builtin_ia32_sfence();
    return true;
}

uint64_t GuestRegion::length() const
{
    AV_SCOPE("GuestRegion::length");
    return memory_ != nullptr ? memory_->size() : 0u;
}

/* Only memory the source maps is a region: one with none could answer no
 * offset. */
bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    return memory_ == nullptr || (memory_->pointer() != nullptr && memory_->size() != 0u);
}

}  // namespace av
