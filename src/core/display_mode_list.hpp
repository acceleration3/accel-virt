/* The resolutions the adapter's one output supports, until the device reports
 * its displays (feature 027): common modes up to 1920x1080, 1280x800 preferred
 * because it is QEMU virtio-gpu's default. */
#ifndef AV_DISPLAY_MODE_LIST_HPP
#define AV_DISPLAY_MODE_LIST_HPP

#include "display_mode.hpp"

#include <stdint.h>

namespace av {

class DisplayModeList {
public:
    static constexpr uint32_t mode_count = 11u;

    DisplayModeList();
    DisplayModeList(const DisplayModeList &) = delete;
    DisplayModeList &operator=(const DisplayModeList &) = delete;

    uint32_t           count() const;
    /* The first mode for an index past the end. */
    const DisplayMode &mode(uint32_t index) const;
    const DisplayMode &preferred() const;
    bool               is_preferred(const DisplayMode &mode) const;
    /* Null when no mode in the list matches. */
    const DisplayMode *find(const D3dkmdtVidPnSourceModeRecord &source) const;
    const DisplayMode *find(const D3dkmdtVideoSignalInfoRecord &signal) const;

private:
    static constexpr uint32_t preferred_index = 4u;

    DisplayMode modes_[mode_count];
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_LIST_HPP
