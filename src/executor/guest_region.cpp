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

bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    return bytes_.invariants_hold();
}

}  // namespace av
