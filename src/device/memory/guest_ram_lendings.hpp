/* Every lending of guest RAM the devices of one QEMU process hold
 * (GuestRamLending), by the pages they lie over. The one place that decides
 * whether pages a device is lent are new memory or memory another adapter's
 * device lent already: Direct3D 11, Direct3D 12 and every other API lend
 * through the same call (HostWindowChunk::adopt, docs/host-memory.md) and
 * none of them knows the difference. Owned by the module, outliving every
 * device. Main loop. */
#ifndef AV_GUEST_RAM_LENDINGS_HPP
#define AV_GUEST_RAM_LENDINGS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/memory/guest_ram_lending.hpp"
#include "device/memory/lending_life.hpp"
#include "device/memory/lent_pages.hpp"
#include "device/memory/lent_run.hpp"
#include "device/memory/run_swap_gate.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextPlane;
class ControlRequest;
class Host;

class GuestRamLendings {
public:
    explicit GuestRamLendings(BlockSource &blocks);
    GuestRamLendings(const GuestRamLendings &) = delete;
    GuestRamLendings &operator=(const GuestRamLendings &) = delete;

    /* A new lending of `size` bytes of the dma-buf `fd`, which it owns from
     * now on (closed through `host` if it cannot be made), held by blob `key`
     * of `plane`; null when there is no room. */
    GuestRamLending *lend(Host &host, uint64_t size, int fd, ContextPlane &plane, uint64_t key);
    /* The lending already laid whole over the pages from `first_address` of
     * `size` bytes, now also held by blob `key` of `plane`; null when there is
     * none, or it is not that, or is held by no plane but this one (the same
     * device lending the same pages twice is the refusal it always was), or
     * has no room for one holder more. */
    GuestRamLending *join(uint64_t first_address, uint64_t size, ContextPlane &plane, uint64_t key);
    /* The lending whose memory the `entries` page runs named at `offset` of
     * `request` lie over, whole and in that order, and where: ok with `run`
     * set; not_found when the first run is under no lending, or one that is
     * not wholly laid; not_supported when the runs are not one piece of its
     * memory in order; invalid_parameter for a list the request does not
     * hold. Nothing is held or changed. */
    [[nodiscard]] Status locate(ControlRequest &request, size_t offset, uint32_t entries, LentRun &run) const;
    /* Blob `key` of `plane` gone from `lending`; the pages given back their
     * RAM, and the lending gone, once no blob holds it. */
    void             leave(Host &host, GuestRamLending &lending, const ContextPlane &plane, uint64_t key);
    uint32_t         live() const;
    /* The smallest id of a live lending; 0 when there is none. Ids only grow,
     * so one below it is of a lending given back. */
    uint64_t         oldest_live() const;
    /* `lending` kept watchable: its LendingLife outlives it until the
     * watcher lets go with unwatch. Main loop. */
    LendingLife     *watch(GuestRamLending &lending);
    void             unwatch(LendingLife *life);
    /* Who holds the writers still while runs are swapped. */
    RunSwapGate     &swaps();
    uint64_t         pages() const;

    bool invariants_hold() const;

private:
    LentPages                    pages_;
    RunSwapGate                  swaps_;
    /* Declared before the lendings, which hold one each. */
    ObjectPool<LendingLife>      lives_;
    ObjectPool<GuestRamLending>  lendings_;
    BlockSource                 &blocks_;
    /* The last id a lending was given. */
    uint64_t                     serial_;
};

}  // namespace av

#endif  // AV_GUEST_RAM_LENDINGS_HPP
