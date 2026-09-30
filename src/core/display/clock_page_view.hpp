/* The display clock's page as the guest holds it (docs/protocol.md, "Display
 * clock"): a page of the driver's contiguous memory the device is told of and
 * writes each head's vertical blanks in. Everything on it is the device's word
 * and read as hostile - a header this driver does not know, a record that is
 * never whole, a rate out of range - except interrupt_every, which is the
 * driver's. */
#ifndef AV_CLOCK_PAGE_VIEW_HPP
#define AV_CLOCK_PAGE_VIEW_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VblankRecord;

class ClockPageView {
public:
    /* A device writes a record in a few stores; reading one mid-write this
     * many times in a row is a device that never stops. */
    static constexpr uint32_t read_attempts = 64u;

    explicit ClockPageView(Platform &platform);
    ~ClockPageView();
    ClockPageView(const ClockPageView &) = delete;
    ClockPageView &operator=(const ClockPageView &) = delete;

    /* PASSIVE: the page, zeroed. no_memory without contiguous memory;
     * invalid_state when it is already there. */
    [[nodiscard]] Status initialize();
    void                 release();
    /* The page's physical address, what DISPLAY_CLOCK names; 0 without one. */
    uint64_t             address() const;
    /* Dispatch at most. Head `head`'s latest blank: invalid_state without a
     * page, malformed for a header or record this driver does not accept,
     * not_found for a head the page does not describe, conflict when no read
     * found the record whole. `blank` is unchanged unless ok. */
    [[nodiscard]] Status read(uint32_t head, VblankRecord &blank) const;
    /* Dispatch at most. The device raises the display line on head `head`'s
     * blanks whose sequence is a multiple of `every`, and on none for 0.
     * Nothing without a page or for a head past the page's. */
    void                 ask(uint32_t head, uint32_t every);

    bool invariants_hold() const;

private:
    /* The 4-byte word at `offset` of the page. */
    uint32_t *word(uint64_t offset) const;
    /* The heads the device says the page has; 0 for a header it cannot trust. */
    uint32_t  heads() const;

    Platform &platform_;
    uint8_t  *page_;
};

}  // namespace av

#endif  // AV_CLOCK_PAGE_VIEW_HPP
