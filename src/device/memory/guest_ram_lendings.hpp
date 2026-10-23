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
#include "device/memory/lent_pages.hpp"

#include <stdint.h>

namespace av {

class ContextPlane;
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
    /* Blob `key` of `plane` gone from `lending`; the pages given back their
     * RAM, and the lending gone, once no blob holds it. */
    void             leave(Host &host, GuestRamLending &lending, const ContextPlane &plane, uint64_t key);
    uint32_t         live() const;
    uint64_t         pages() const;

    bool invariants_hold() const;

private:
    LentPages                    pages_;
    ObjectPool<GuestRamLending>  lendings_;
    BlockSource                 &blocks_;
};

}  // namespace av

#endif  // AV_GUEST_RAM_LENDINGS_HPP
