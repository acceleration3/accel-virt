#ifndef AV_DISPLAY_MODES_HPP
#define AV_DISPLAY_MODES_HPP

#include "core/support/status.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

namespace av {

class Host;

class DisplayModes {
public:
    explicit DisplayModes(Host &host);
    DisplayModes(const DisplayModes &) = delete;
    DisplayModes &operator=(const DisplayModes &) = delete;

    [[nodiscard]] Status set(const char *text);
    [[nodiscard]] Status set_refresh(uint32_t millihz);
    const DisplayModeSet &current() const;

private:
    Host          &host_;
    DisplayModeSet modes_;
};

}

#endif
