#ifndef AV_LENT_PAGES_HPP
#define AV_LENT_PAGES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class GuestRamLending;
struct LentPagesProbe;

class LentPages {
public:
    static constexpr uint64_t page_size = 4096u;

    explicit LentPages(BlockSource &blocks);
    LentPages(const LentPages &) = delete;
    LentPages &operator=(const LentPages &) = delete;

    [[nodiscard]] Status claim(uint64_t address, uint64_t bytes, GuestRamLending &owner);
    void                 release(uint64_t address, uint64_t bytes);
    bool                 lent(uint64_t address) const;
    GuestRamLending     *owner_of(uint64_t address) const;
    uint64_t             count() const;

    bool invariants_hold() const;

private:
    using Owners = KeyedSlots<uint64_t, GuestRamLending *>;

    Owners pages_;

    friend struct LentPagesProbe;
};

}

#endif
