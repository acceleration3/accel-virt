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

    GuestRamLending *lend(Host &host, uint64_t size, int fd, ContextPlane &plane, uint64_t key);
    GuestRamLending *join(uint64_t first_address, uint64_t size, ContextPlane &plane, uint64_t key);
    void             leave(Host &host, GuestRamLending &lending, const ContextPlane &plane, uint64_t key);
    uint32_t         live() const;
    uint64_t         pages() const;

    bool invariants_hold() const;

private:
    LentPages                    pages_;
    ObjectPool<GuestRamLending>  lendings_;
    BlockSource                 &blocks_;
};

}

#endif
