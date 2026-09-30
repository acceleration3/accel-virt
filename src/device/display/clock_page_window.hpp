/* The guest page the display clock is published in (docs/protocol.md,
 * "Display clock"), mapped for the device once, on the main loop, and written
 * from the clock thread. A head's record is written under its generation, so
 * a guest reading while it changes sees an odd or a different number and
 * reads again; the one field the guest writes, interrupt_every, is read here
 * as the hostile value it is. */
#ifndef AV_CLOCK_PAGE_WINDOW_HPP
#define AV_CLOCK_PAGE_WINDOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class VblankRecord;

class ClockPageWindow {
public:
    explicit ClockPageWindow(Host &host);
    ~ClockPageWindow();
    ClockPageWindow(const ClockPageWindow &) = delete;
    ClockPageWindow &operator=(const ClockPageWindow &) = delete;

    /* Main loop. The page at `address`, headed for `heads` records, each
     * zero. not_found for an address that is not mapped, not_supported for
     * memory that is not guest RAM or a mapping shorter than the page,
     * invalid_parameter for a head count no page describes. Whatever was
     * mapped before is let go first, and nothing is held on failure. */
    [[nodiscard]] Status map(uint64_t address, uint32_t heads);
    void                 release();
    bool                 mapped() const;
    /* Any thread, while mapped: head `head`'s record is `blank`. Nothing for a
     * head past the page's. */
    void                 publish(uint32_t head, const VblankRecord &blank);
    /* Any thread, while mapped: what the guest last wrote in head `head`'s
     * interrupt_every; 0 past the page's heads or while unmapped. */
    uint32_t             interrupt_every(uint32_t head) const;

    bool invariants_hold() const;

private:
    /* The 4-byte word at `offset` of the page. */
    uint32_t *word(uint64_t offset) const;

    Host    &host_;
    uint8_t *page_;
    uint32_t heads_;
};

}  // namespace av

#endif  // AV_CLOCK_PAGE_WINDOW_HPP
