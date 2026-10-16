/* Every guest page lent to the device and laid under a host blob, by its
 * guest physical address (docs/host-memory.md, "Application memory"): a page
 * can be laid under one blob at a time, since a second overlay would copy the
 * first blob's memory into its own and the first's restore would then take
 * the page from under it. Two of a process's ranges sharing a page - OpenCL
 * buffers over neighbouring bytes of one allocation - are told apart here,
 * and the second is refused. Main loop. */
#ifndef AV_LENT_PAGES_HPP
#define AV_LENT_PAGES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
struct LentPagesProbe;

class LentPages {
public:
    static constexpr uint64_t page_size = 4096u;

    explicit LentPages(Host &host);
    LentPages(const LentPages &) = delete;
    LentPages &operator=(const LentPages &) = delete;

    /* The pages of [address, address + bytes) - whole pages - marked lent:
     * conflict, marking none, when one already is; no_memory, marking none,
     * when the set cannot grow. */
    [[nodiscard]] Status claim(uint64_t address, uint64_t bytes);
    /* The pages of a range claim marked, lent no more. */
    void                 release(uint64_t address, uint64_t bytes);
    bool                 lent(uint64_t address) const;
    uint64_t             count() const;

    bool invariants_hold() const;

private:
    /* By page number; the entry says nothing. */
    KeyedSlots<uint64_t, uint8_t> pages_;

    friend struct LentPagesProbe;
};

}  // namespace av

#endif  // AV_LENT_PAGES_HPP
