/* DISPLAY_TIMING (docs/protocol.md, "Display clock"): the refresh rate a head's
 * vertical blanks run at from its next one. The header, then the head and the
 * rate in millihertz. Which heads exist is the device's to judge. */
#ifndef AV_DISPLAY_TIMING_REQUEST_HPP
#define AV_DISPLAY_TIMING_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class DisplayTimingRequest {
public:
    static constexpr uint32_t type = 0x02F5u;
    static constexpr size_t   encoded_size = 32u;
    /* 1 Hz to 1 kHz: slower is no display, faster no host wakes for. */
    static constexpr uint32_t least_millihz = 1000u;
    static constexpr uint32_t most_millihz = 1000000u;

    DisplayTimingRequest();

    /* malformed for a request too short to hold both, or a rate out of range. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             head() const;
    uint32_t             refresh_millihz() const;

private:
    static constexpr size_t header_size = 24u;

    uint32_t head_;
    uint32_t refresh_millihz_;
};

}  // namespace av

#endif  // AV_DISPLAY_TIMING_REQUEST_HPP
