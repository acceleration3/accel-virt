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
    static constexpr uint32_t max_modes = DisplayModeSet::max_modes;
    static_assert(standard_count + 1u <= max_modes, "the standard list and the device's size fit");

    DisplayModeList();
    DisplayModeList(const DisplayModeList &) = delete;
    DisplayModeList &operator=(const DisplayModeList &) = delete;

    [[nodiscard]] Status follow(const DisplayInfoResponse &info, uint32_t scanout);
    [[nodiscard]] Status adopt(const DisplayModeSet &set);

    uint32_t           count() const;
    const DisplayMode &mode(uint32_t index) const;
    const DisplayMode &preferred() const;
    bool               is_preferred(const DisplayMode &mode) const;
    const DisplayMode *find(const D3dkmdtVidPnSourceModeRecord &source) const;
    const DisplayMode *find(const D3dkmdtVideoSignalInfoRecord &signal) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t default_preferred_index = 4u;

    uint64_t area_of(const DisplayMode &mode) const;

    DisplayMode standard_[standard_count];
    DisplayMode modes_[max_modes];
    uint32_t    count_;
    uint32_t    preferred_index_;

    friend struct DisplayModeListProbe;
};

}

#endif
