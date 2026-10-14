/* The colour a probe's picture is judged against: a pixel is it when every
 * channel is within one of it. A shader's 0.5 is 127.5 as a UNORM byte, which
 * a driver may round either way - NVIDIA's writes 127 where Intel's write 128 -
 * and a picture read back off the window is the same bytes, so every judge of
 * one draws the same line. */
#ifndef AV_PROBE_COLOUR_HPP
#define AV_PROBE_COLOUR_HPP

#include <stdint.h>

namespace av {

class ProbeColour {
public:
    static constexpr uint32_t max_channels = 4u;

    /* The first `channels` bytes of `colour` (at most max_channels), in the
     * order the pictures this judges hold them. */
    ProbeColour(const uint8_t *colour, uint32_t channels);

    /* How many of the `count` pixels at `pixels`, `pixel_bytes` apart, are
     * this colour; bytes of a pixel past the channels are not looked at. */
    uint32_t matched(const uint8_t *pixels, uint32_t count, uint32_t pixel_bytes) const;

private:
    bool is(const uint8_t *pixel) const;

    uint8_t  colour_[max_channels];
    uint32_t channels_;
};

}  // namespace av

#endif  // AV_PROBE_COLOUR_HPP
