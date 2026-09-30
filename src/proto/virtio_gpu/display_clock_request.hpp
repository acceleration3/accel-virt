/* DISPLAY_CLOCK (docs/protocol.md, "Display clock"): the guest page the device
 * publishes each head's vertical blanks in from now on, or none. A virtio-gpu
 * control request of this driver's own type: the header, then the page's
 * guest physical address. */
#ifndef AV_DISPLAY_CLOCK_REQUEST_HPP
#define AV_DISPLAY_CLOCK_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class DisplayClockRequest {
public:
    static constexpr uint32_t type = 0x02F4u;
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint64_t page_size = 4096u;

    DisplayClockRequest();

    /* malformed for a request too short to hold the address, or an address
     * that is not a page's; 0 is the one that stops publishing. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint64_t             address() const;

private:
    static constexpr size_t header_size = 24u;

    uint64_t address_;
};

}  // namespace av

#endif  // AV_DISPLAY_CLOCK_REQUEST_HPP
