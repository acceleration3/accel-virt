#include "executor/guest_region.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/source_memory.hpp"
#include "proto/wire/guest_bytes.hpp"

namespace av {

GuestRegion::GuestRegion(GuestBytes &bytes) : bytes_(bytes)
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

bool GuestRegion::adopt(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("GuestRegion::adopt");
    handle = 0u;
    if (!bytes_.bind(memory.pointer(), memory.size())) return false;
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory.pointer()));
    return true;
}

void GuestRegion::abandon(const SourceMemory &memory)
{
    AV_SCOPE("GuestRegion::abandon");
    if (is(memory)) bytes_.unbind();
}

bool GuestRegion::is(const SourceMemory &memory) const
{
    AV_SCOPE("GuestRegion::is");
    return bytes_.over(memory.pointer());
}

bool GuestRegion::held() const
{
    AV_SCOPE("GuestRegion::held");
    return bytes_.length() != 0u;
}

bool GuestRegion::publish(uint64_t offset, uint64_t value)
{
    AV_SCOPE("GuestRegion::publish");
    uint8_t *const word = bytes_.at(offset, sizeof(uint64_t));
    if (word == nullptr || reinterpret_cast<uintptr_t>(word) % sizeof(uint64_t) != 0u) return false;
    __atomic_store_n(reinterpret_cast<uint64_t *>(word), value, __ATOMIC_RELEASE);
    /* The region is the memory source's mapping, which a driver may have made
     * write-combined (docs/host-memory.md): x86 orders such a store with the
     * device's own - the batch head the guest reads before the word - only
     * at a store fence. */
    __builtin_ia32_sfence();
    return true;
}

bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    return bytes_.invariants_hold();
}

}  // namespace av
