#include "device/memory/guest_ram_lendings.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

GuestRamLendings::GuestRamLendings(BlockSource &blocks) : pages_(blocks), lendings_(blocks), blocks_(blocks)
{
    AV_SCOPE("GuestRamLendings::GuestRamLendings");
}

GuestRamLending *GuestRamLendings::lend(Host &host, uint64_t size, int fd, ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLendings::lend");
    GuestRamLending *made = nullptr;
    if (lendings_.create(made, blocks_, pages_, size, fd, plane, key) == Status::ok) return made;
    host.close_descriptor(fd);
    return nullptr;
}

GuestRamLending *GuestRamLendings::join(uint64_t first_address, uint64_t size, ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLendings::join");
    GuestRamLending *const found = pages_.owner_of(first_address);
    if (found == nullptr || found->start() != first_address || found->size() != size || !found->complete() ||
        !found->held_beyond(plane) || found->join(plane, key) != Status::ok)
        return nullptr;
    return found;
}

void GuestRamLendings::leave(Host &host, GuestRamLending &lending, const ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLendings::leave");
    if (lending.leave(plane, key) != 0u) return;
    lending.give_back(host);
    static_cast<void>(lendings_.destroy(&lending));
}

uint32_t GuestRamLendings::live() const
{
    AV_SCOPE("GuestRamLendings::live");
    return lendings_.live();
}

uint64_t GuestRamLendings::pages() const
{
    AV_SCOPE("GuestRamLendings::pages");
    return pages_.count();
}

bool GuestRamLendings::invariants_hold() const
{
    AV_SCOPE("GuestRamLendings::invariants_hold");
    bool held = pages_.invariants_hold() && lendings_.invariants_hold();
    lendings_.for_each([&held](const GuestRamLending &lending) { held = held && lending.invariants_hold(); });
    return held;
}

}
