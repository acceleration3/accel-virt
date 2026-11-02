#include "guest/probe/probe_colour.hpp"

#include "core/trace/function_scope.hpp"

#include <stddef.h>

namespace av {

ProbeColour::ProbeColour(const uint8_t *colour, uint32_t channels)
    : colour_{}, channels_(channels < max_channels ? channels : max_channels)
{
    AV_SCOPE("ProbeColour::ProbeColour");
    for (uint32_t channel = 0u; channel < channels_; channel++) colour_[channel] = colour[channel];
}

uint32_t ProbeColour::matched(const uint8_t *pixels, uint32_t count, uint32_t pixel_bytes) const
{
    AV_SCOPE("ProbeColour::matched");
    uint32_t matches = 0u;
    for (uint32_t pixel = 0u; pixel < count; pixel++) matches += is(pixels + size_t{pixel} * pixel_bytes) ? 1u : 0u;
    return matches;
}

bool ProbeColour::is(const uint8_t *pixel) const
{
    AV_SCOPE("ProbeColour::is");
    for (uint32_t channel = 0u; channel < channels_; channel++) {
        const int32_t difference = static_cast<int32_t>(pixel[channel]) - static_cast<int32_t>(colour_[channel]);
        if (difference < -1 || difference > 1) return false;
    }
    return true;
}

}
