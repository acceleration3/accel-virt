#ifndef AV_HOST_MONITOR_MODES_HPP
#define AV_HOST_MONITOR_MODES_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DisplayModeSet;
class Host;

class HostMonitorModes {
public:
    explicit HostMonitorModes(Host &host);
    HostMonitorModes(const HostMonitorModes &) = delete;
    HostMonitorModes &operator=(const HostMonitorModes &) = delete;

    [[nodiscard]] Status read(DisplayModeSet &modes);

private:
    static constexpr size_t   listing_bytes = 4096u;
    static constexpr size_t   file_bytes = 4096u;
    static constexpr size_t   path_bytes = 160u;
    static constexpr uint32_t least_width = 640u;
    static constexpr uint32_t least_height = 480u;

    bool connected(const char *name);
    void add_modes(const char *name, DisplayModeSet &modes, uint32_t &best_width, uint32_t &best_height);

    Host &host_;
};

}

#endif
