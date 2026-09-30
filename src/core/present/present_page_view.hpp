/* The present engine's page as the guest holds it (docs/protocol.md, "Present
 * engine"): a page of the driver's contiguous memory the device is told of and
 * counts the presents it has finished with in. Everything on it is the
 * device's word and read as hostile - a header this driver does not know, or
 * more refused than completed, is a page that says nothing. */
#ifndef AV_PRESENT_PAGE_VIEW_HPP
#define AV_PRESENT_PAGE_VIEW_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentProgress;

class PresentPageView {
public:
    explicit PresentPageView(Platform &platform);
    ~PresentPageView();
    PresentPageView(const PresentPageView &) = delete;
    PresentPageView &operator=(const PresentPageView &) = delete;

    /* PASSIVE: the page, zeroed. no_memory without contiguous memory;
     * invalid_state when it is already there. */
    [[nodiscard]] Status initialize();
    /* PASSIVE: zeroed again, for a device about to be told of it anew. */
    void                 clear();
    /* The page's physical address, what PRESENT_ENGINE names; 0 without one. */
    uint64_t             address() const;
    /* Dispatch at most. What the device last wrote: invalid_state without a
     * page, malformed for one this driver does not accept. `progress` is
     * unchanged unless ok. */
    [[nodiscard]] Status read(PresentProgress &progress) const;

private:
    /* The 4-byte word at `offset` of the page. */
    uint32_t *word(uint64_t offset) const;

    Platform &platform_;
    uint8_t  *page_;
};

}  // namespace av

#endif  // AV_PRESENT_PAGE_VIEW_HPP
