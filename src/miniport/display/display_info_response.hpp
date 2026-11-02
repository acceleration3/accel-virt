#ifndef AV_DISPLAY_INFO_RESPONSE_HPP
#define AV_DISPLAY_INFO_RESPONSE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DisplayInfoResponseProbe;

class DisplayInfoResponse {
public:
    static constexpr uint32_t max_scanouts = 16u;
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   scanout_size = 24u;
    static constexpr size_t   encoded_size = header_size + max_scanouts * scanout_size;
    static constexpr uint32_t response_type = 0x1101u;
    static constexpr uint32_t max_dimension = 16384u;

    DisplayInfoResponse();

    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    uint32_t enabled_count() const;
    bool     enabled(uint32_t scanout) const;
    uint32_t width(uint32_t scanout) const;
    uint32_t height(uint32_t scanout) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status read_scanout(class WireReader &reader, uint32_t index, uint32_t *widths, uint32_t *heights,
                                      uint64_t &enabled) const;

    uint32_t widths_[max_scanouts];
    uint32_t heights_[max_scanouts];
    uint64_t enabled_;

    friend struct DisplayInfoResponseProbe;
};

}

#endif
