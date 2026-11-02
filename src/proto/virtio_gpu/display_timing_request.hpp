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
    static constexpr uint32_t least_millihz = 1000u;
    static constexpr uint32_t most_millihz = 1000000u;

    DisplayTimingRequest();

    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             head() const;
    uint32_t             refresh_millihz() const;

private:
    static constexpr size_t header_size = 24u;

    uint32_t head_;
    uint32_t refresh_millihz_;
};

}

#endif
