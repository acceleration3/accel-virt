#ifndef AV_PROBE_COLOUR_HPP
#define AV_PROBE_COLOUR_HPP

#include <stdint.h>

namespace av {

class ProbeColour {
public:
    static constexpr uint32_t max_channels = 4u;

    ProbeColour(const uint8_t *colour, uint32_t channels);

    uint32_t matched(const uint8_t *pixels, uint32_t count, uint32_t pixel_bytes) const;

private:
    bool is(const uint8_t *pixel) const;

    uint8_t  colour_[max_channels];
    uint32_t channels_;
};

}

#endif
