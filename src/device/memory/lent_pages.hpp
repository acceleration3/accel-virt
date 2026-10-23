/* Every guest page lent to a device of this process and laid under a host
 * blob, by its guest physical address, and the lending it lies under
 * (docs/host-memory.md, "Application memory"): a page can be laid under one
 * lending at a time, since a second overlay would copy the first one's memory
 * into its own and the first's restore would then take the page from under
 * it. Two of a process's ranges sharing a page - OpenCL buffers over
 * neighbouring bytes of one allocation - are told apart here, and the second
 * is refused; the same pages lent again by a second adapter's device are not
 * a second lending but one both hold (GuestRamLendings). Main loop. */
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

    /* The pages of [address, address + bytes) - whole pages - marked lent
     * under `owner`: conflict, marking none, when one already is; no_memory,
     * marking none, when the set cannot grow. */
    [[nodiscard]] Status claim(uint64_t address, uint64_t bytes, GuestRamLending &owner);
    /* The pages of a range claim marked, lent no more. */
    void                 release(uint64_t address, uint64_t bytes);
    bool                 lent(uint64_t address) const;
    /* The lending the page at `address` lies under; null for a page not lent. */
    GuestRamLending     *owner_of(uint64_t address) const;
    uint64_t             count() const;

    bool invariants_hold() const;

private:
    using Owners = KeyedSlots<uint64_t, GuestRamLending *>;

    /* By page number. */
    Owners pages_;

    friend struct LentPagesProbe;
};

}  // namespace av

#endif  // AV_LENT_PAGES_HPP
