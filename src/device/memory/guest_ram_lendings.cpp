#include "device/memory/guest_ram_lendings.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GuestRamLendings::GuestRamLendings(BlockSource &blocks) : pages_(blocks), swaps_(blocks), lives_(blocks), lendings_(blocks), blocks_(blocks), serial_(0u)
{
    AV_SCOPE("GuestRamLendings::GuestRamLendings");
}

GuestRamLending *GuestRamLendings::lend(Host &host, uint64_t size, int fd, ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLendings::lend");
    GuestRamLending *made = nullptr;
    LendingLife     *life = nullptr;
    if (lives_.create(life) == Status::ok) {
        if (lendings_.create(made, blocks_, pages_, swaps_, *life, ++serial_, size, fd, plane, key) == Status::ok)
            return made;
        static_cast<void>(lives_.destroy(life));
    }
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

Status GuestRamLendings::locate(ControlRequest &request, size_t offset, uint32_t entries, LentRun &run) const
{
    AV_SCOPE("GuestRamLendings::locate");
    constexpr size_t entry_size = GuestPageOverlay::entry_size;
    GuestRamLending *lending = nullptr;
    uint64_t         start = 0u, next = 0u, total = 0u;
    uint32_t         hint = 0u;
    for (uint32_t index = 0u; index < entries; index++) {
        uint8_t entry[entry_size] = {};
        if (request.read(offset + index * entry_size, entry, sizeof entry) != sizeof entry)
            return Status::invalid_parameter;
        WireReader     reader(entry, sizeof entry);
        const uint64_t address = reader.read_u64();
        const uint64_t length = reader.read_u32();
        /* Whole pages, as every entry a lending lays is: the offset a copy
         * lands at is then as aligned as the pages. */
        if (address % LentPages::page_size != 0u || length % LentPages::page_size != 0u)
            return Status::not_supported;
        if (index == 0u) {
            lending = pages_.owner_of(address);
            if (lending == nullptr || !lending->complete()) return Status::not_found;
        }
        uint64_t where = 0u;
        if (!lending->locate(address, length, hint, where) || (index != 0u && where != next))
            return Status::not_supported;
        if (index == 0u) start = where;
        next = where + length;
        total += length;
    }
    if (lending == nullptr) return Status::invalid_parameter;
    run = LentRun(*lending, start, total);
    return Status::ok;
}

void GuestRamLendings::leave(Host &host, GuestRamLending &lending, const ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLendings::leave");
    if (lending.leave(plane, key) != 0u) return;
    /* Said before the RAM is given back, so a present that finds it clear is
     * copying into pages the guest still has. */
    LendingLife &life = lending.life();
    life.give_back();
    lending.give_back(host);
    static_cast<void>(lendings_.destroy(&lending));
    unwatch(&life);
}

LendingLife *GuestRamLendings::watch(GuestRamLending &lending)
{
    AV_SCOPE("GuestRamLendings::watch");
    lending.life().hold();
    return &lending.life();
}

void GuestRamLendings::unwatch(LendingLife *life)
{
    AV_SCOPE("GuestRamLendings::unwatch");
    if (life != nullptr && life->let_go() == 0u) static_cast<void>(lives_.destroy(life));
}

RunSwapGate &GuestRamLendings::swaps()
{
    AV_SCOPE("GuestRamLendings::swaps");
    return swaps_;
}

uint64_t GuestRamLendings::oldest_live() const
{
    AV_SCOPE("GuestRamLendings::oldest_live");
    uint64_t oldest = 0u;
    lendings_.for_each([&oldest](const GuestRamLending &lending) {
        if (oldest == 0u || lending.id() < oldest) oldest = lending.id();
    });
    return oldest;
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

}  // namespace av
