/* The guest page the present engine says how far it got in (docs/protocol.md,
 * "Present engine"), mapped for the device once, on the main loop, and written
 * from the engine's thread and the main loop both, under the engine's lock.
 * Each count is one aligned eight-byte store, completed before refused, so a
 * guest that reads refused and then completed never reads more refused than
 * completed. */
#ifndef AV_PRESENT_PAGE_WINDOW_HPP
#define AV_PRESENT_PAGE_WINDOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class PresentProgress;

class PresentPageWindow {
public:
    explicit PresentPageWindow(Host &host);
    ~PresentPageWindow();
    PresentPageWindow(const PresentPageWindow &) = delete;
    PresentPageWindow &operator=(const PresentPageWindow &) = delete;

    /* Main loop. The page at `address`, headed and counting nothing. not_found
     * for an address that is not mapped, not_supported for memory that is not
     * guest RAM or a mapping shorter than the page. Whatever was mapped before
     * is let go first, and nothing is held on failure. */
    [[nodiscard]] Status map(uint64_t address);
    void                 release();
    bool                 mapped() const;
    /* Any thread, while mapped: the page says `progress`. */
    void                 publish(const PresentProgress &progress);

private:
    /* The eight-byte word at `offset` of the page. */
    uint64_t *word(uint64_t offset) const;

    Host    &host_;
    uint8_t *page_;
};

}  // namespace av

#endif  // AV_PRESENT_PAGE_WINDOW_HPP
