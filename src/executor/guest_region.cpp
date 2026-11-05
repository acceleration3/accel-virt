#include "executor/guest_region.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/source_memory.hpp"
#include "proto/submission/blob_role.hpp"

namespace av {

GuestRegion::GuestRegion(GuestBytes &bytes, DriverLog &lines) : bytes_(bytes), lines_(lines), memories_{}
{
    AV_SCOPE("GuestRegion::GuestRegion");
}

bool GuestRegion::adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle)
{
    AV_SCOPE("GuestRegion::adopt");
    handle = 0u;
    if (!role.is_arena()) return false;
    const Status bound = bytes_.bind(role.base(), memory.pointer(), memory.size());
    if (bound != Status::ok) {
        LogLine(lines_)
            .text("an arena segment of ")
            .number(memory.size())
            .text(" bytes would not bind at ")
            .number(role.base())
            .text(", status ")
            .number(static_cast<uint32_t>(bound))
            .write();
        return false;
    }
    memories_[role.base() >> GuestBytes::slot_shift] = &memory;
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory.pointer()));
    return true;
}

void GuestRegion::abandon(const SourceMemory &memory)
{
    AV_SCOPE("GuestRegion::abandon");
    const uint64_t base = bytes_.base_of(memory.pointer());
    if (base == GuestBytes::no_base || bytes_.unbind(memory.pointer()) != Status::ok) return;
    memories_[base >> GuestBytes::slot_shift] = nullptr;
}

bool GuestRegion::is(const SourceMemory &memory) const
{
    AV_SCOPE("GuestRegion::is");
    return base_of(memory) != GuestBytes::no_base;
}

bool GuestRegion::held() const
{
    AV_SCOPE("GuestRegion::held");
    return bytes_.segments() != 0u;
}

uint64_t GuestRegion::base_of(const SourceMemory &memory) const
{
    AV_SCOPE("GuestRegion::base_of");
    return bytes_.base_of(memory.pointer());
}

SourceMemory *GuestRegion::memory_at(uint64_t offset) const
{
    AV_SCOPE("GuestRegion::memory_at");
    const uint64_t base = bytes_.base_at(offset);
    return base == GuestBytes::no_base ? nullptr : memories_[base >> GuestBytes::slot_shift];
}

bool GuestRegion::invariants_hold() const
{
    AV_SCOPE("GuestRegion::invariants_hold");
    if (!bytes_.invariants_hold()) return false;
    uint32_t held_memories = 0u;
    for (uint32_t slot = 0u; slot < GuestBytes::slot_count; slot++) {
        if (memories_[slot] == nullptr) continue;
        if (bytes_.base_of(memories_[slot]->pointer()) != (uint64_t{slot} << GuestBytes::slot_shift)) return false;
        held_memories++;
    }
    return held_memories == bytes_.segments();
}

}
