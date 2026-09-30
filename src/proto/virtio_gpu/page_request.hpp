/* A control request of this driver's own naming one guest page the device
 * writes from now on, or none: the header, then the page's guest physical
 * address. The display clock publishes each head's vertical blanks in its page
 * (docs/protocol.md, "Display clock"), the present engine the presents it has
 * moved ("Present engine"). */
#ifndef AV_PAGE_REQUEST_HPP
#define AV_PAGE_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class PageRequest {
public:
    static constexpr uint32_t display_clock = 0x02F4u;
    static constexpr uint32_t present_engine = 0x02F6u;
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint64_t page_size = 4096u;

    PageRequest();

    /* malformed for a request too short to hold the address, or an address
     * that is not a page's; 0 is the one that stops publishing. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint64_t             address() const;

private:
    static constexpr size_t header_size = 24u;

    uint64_t address_;
};

}  // namespace av

#endif  // AV_PAGE_REQUEST_HPP
