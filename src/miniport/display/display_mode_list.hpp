/* The resolutions this output offers: the host's own (GET_DISPLAY_MODES) when
 * the device has them - any sizes, standard or not - or else the size the
 * device reports for its scanout and the standard modes that fit inside it.
 * Until the device is asked, the standard list stands on its own with 1280x800
 * preferred, which is QEMU virtio-gpu's default. */
#ifndef AV_DISPLAY_MODE_LIST_HPP
#define AV_DISPLAY_MODE_LIST_HPP

#include "miniport/display/display_mode.hpp"
#include "core/support/status.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <stdint.h>

namespace av {

class DisplayInfoResponse;
struct DisplayModeListProbe;

class DisplayModeList {
public:
    static constexpr uint32_t standard_count = 11u;
    /* The host's modes, at most a set's; the standard ones and the device's own
     * size fit too. */
    static constexpr uint32_t max_modes = DisplayModeSet::max_modes;
    static_assert(standard_count + 1u <= max_modes, "the standard list and the device's size fit");

    DisplayModeList();
    DisplayModeList(const DisplayModeList &) = delete;
    DisplayModeList &operator=(const DisplayModeList &) = delete;

    /* Offers what fits the rectangle `scanout` reports, and prefers that
     * rectangle. not_found for a scanout the device does not report, which
     * leaves the list as it was: a driver with no modes offers no display. */
    [[nodiscard]] Status follow(const DisplayInfoResponse &info, uint32_t scanout);
    /* Offers the host's modes, in ascending area, the set's preferred one
     * preferred. Only offerable modes are taken - one dxgkrnl refuses would
     * fail the whole source mode set, and the display with it; when the set's
     * preferred is not one of them, the largest that is is preferred.
     * not_found for a set with none, which leaves the list as it was. */
    [[nodiscard]] Status adopt(const DisplayModeSet &set);

    uint32_t           count() const;
    /* The first mode for an index past the end. */
    const DisplayMode &mode(uint32_t index) const;
    const DisplayMode &preferred() const;
    bool               is_preferred(const DisplayMode &mode) const;
    /* Null when no mode in the list matches. */
    const DisplayMode *find(const D3dkmdtVidPnSourceModeRecord &source) const;
    const DisplayMode *find(const D3dkmdtVideoSignalInfoRecord &signal) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t default_preferred_index = 4u;

    /* Pixels of a mode, which is how the list stays in ascending order. */
    uint64_t area_of(const DisplayMode &mode) const;

    DisplayMode standard_[standard_count];
    DisplayMode modes_[max_modes];
    uint32_t    count_;
    uint32_t    preferred_index_;

    friend struct DisplayModeListProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_LIST_HPP
