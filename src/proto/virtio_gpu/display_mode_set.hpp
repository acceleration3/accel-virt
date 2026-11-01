/* The resolutions a display offers, as the device sets them and the driver
 * reads them: the answer to GET_DISPLAY_MODES (docs/protocol.md). Any width
 * and height a display could have, not only the standard ones, in the order
 * the device gives them, and one of them preferred - the one the guest shows
 * unless its user picks another. A set with no mode is no set: a display
 * offers at least one. The set also says the one refresh rate the display runs
 * at, in millihertz, whatever mode is shown: after the modes, so a driver that
 * does not read it still reads the modes, and a set without it is 60 Hz. */
#ifndef AV_DISPLAY_MODE_SET_HPP
#define AV_DISPLAY_MODE_SET_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;
struct DisplayModeSetProbe;

class DisplayModeSet {
public:
    static constexpr uint32_t request_type = 0x02F3u;
    static constexpr uint32_t response_type = 0x11F3u;
    static constexpr uint32_t max_modes = 32u;
    /* DisplayInfoResponse's bound: larger than any display, and small enough
     * that no arithmetic on an edge overflows. */
    static constexpr uint32_t max_dimension = 16384u;
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   fixed_size = header_size + 8u;
    static constexpr size_t   mode_size = 8u;
    static constexpr size_t   refresh_size = 4u;
    static constexpr size_t   max_encoded_size = fixed_size + max_modes * mode_size + refresh_size;
    /* The rate of a set that was not told one, and of an answer without one. */
    static constexpr uint32_t default_refresh_millihz = 60000u;
    /* 1 Hz to 1 kHz, the display clock's (DisplayTimingRequest). */
    static constexpr uint32_t least_refresh_millihz = 1000u;
    static constexpr uint32_t most_refresh_millihz = 1000000u;

    DisplayModeSet();

    /* A mode once, in the order added; invalid_parameter for an edge of 0 or
     * past max_dimension, no_space past max_modes. A mode already in the set
     * is not added again, and is ok. */
    [[nodiscard]] Status add(uint32_t width, uint32_t height);
    /* The mode at `index` is preferred; invalid_parameter past the count. */
    [[nodiscard]] Status prefer(uint32_t index);
    /* The refresh for every mode; invalid_parameter outside 1 Hz to 1 kHz.
     * clear() leaves it. */
    [[nodiscard]] Status set_refresh(uint32_t millihz);
    void                 clear();

    uint32_t count() const;
    uint32_t width(uint32_t index) const;
    uint32_t height(uint32_t index) const;
    uint32_t preferred() const;
    uint32_t refresh_millihz() const;
    bool     same_as(const DisplayModeSet &other) const;

    /* The response: header of response_type, then the count, the preferred
     * index, each mode's width and height, and the refresh. Nothing for an
     * empty set. */
    [[nodiscard]] Status encode(WireWriter &writer) const;
    size_t               encoded_size() const;
    /* Leaves the set unchanged unless the whole response holds: its type, a
     * count of 1 to max_modes the bytes carry, a preferred index inside it,
     * and every mode's edges from 1 to max_dimension. A refresh after the
     * modes must be 0 (none) or in range; without one the set is 60 Hz. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    bool invariants_hold() const;

private:
    friend struct DisplayModeSetProbe;

    uint32_t widths_[max_modes];
    uint32_t heights_[max_modes];
    uint32_t count_;
    uint32_t preferred_;
    uint32_t refresh_millihz_;
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_SET_HPP
