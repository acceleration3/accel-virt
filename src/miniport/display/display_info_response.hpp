/* What the device says its displays are: the answer to GET_DISPLAY_INFO
 * (virtio 1.2 5.7.6.7), validated before anything reads it. Every scanout the
 * device reports has a rectangle and an enabled flag; the modes the driver
 * offers are built from the enabled ones, so a rectangle no display could have
 * is refused rather than turned into a mode. */
#ifndef AV_DISPLAY_INFO_RESPONSE_HPP
#define AV_DISPLAY_INFO_RESPONSE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DisplayInfoResponseProbe;

class DisplayInfoResponse {
public:
    static constexpr uint32_t max_scanouts = 16u;   /* VIRTIO_GPU_MAX_SCANOUTS */
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   scanout_size = 24u;   /* struct virtio_gpu_display_one */
    static constexpr size_t   encoded_size = header_size + max_scanouts * scanout_size;
    static constexpr uint32_t response_type = 0x1101u;   /* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
    /* Larger than any display, and small enough that a rectangle's edge cannot
     * overflow the arithmetic that places it. */
    static constexpr uint32_t max_dimension = 16384u;

    DisplayInfoResponse();

    /* malformed for an answer that is not display info, is shorter than one,
     * or reports an enabled scanout with no pixels or more than a display
     * could have. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    uint32_t enabled_count() const;
    /* False for a scanout past the device's, or one it reports as off. */
    bool     enabled(uint32_t scanout) const;
    /* Zero unless that scanout is enabled. */
    uint32_t width(uint32_t scanout) const;
    uint32_t height(uint32_t scanout) const;

    bool invariants_hold() const;

private:
    /* One scanout's rectangle out of `reader`, into the index'th entry of
     * `widths`, `heights` and `enabled`, or malformed. */
    [[nodiscard]] Status read_scanout(class WireReader &reader, uint32_t index, uint32_t *widths, uint32_t *heights,
                                      uint64_t &enabled) const;

    uint32_t widths_[max_scanouts];
    uint32_t heights_[max_scanouts];
    /* A bit per scanout the device reports as on. */
    uint64_t enabled_;

    friend struct DisplayInfoResponseProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_INFO_RESPONSE_HPP
