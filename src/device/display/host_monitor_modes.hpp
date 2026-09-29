/* The modes of the monitors connected to the host, as its kernel lists them
 * under /sys/class/drm: every connected connector's own list, best first,
 * joined into one set, and the best mode of the largest monitor preferred.
 * Interlaced modes, and those smaller than 640x480 - television timings a
 * desktop has no use for - are left out. Main loop. */
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

    /* not_found when the host has no connected monitor, or lists none; `modes`
     * is then as it was. A monitor with more modes than the set holds offers
     * its best ones. */
    [[nodiscard]] Status read(DisplayModeSet &modes);

private:
    static constexpr size_t   listing_bytes = 4096u;
    static constexpr size_t   file_bytes = 4096u;
    static constexpr size_t   path_bytes = 160u;
    static constexpr uint32_t least_width = 640u;
    static constexpr uint32_t least_height = 480u;

    /* Whether the connector `name` has a monitor. */
    bool connected(const char *name);
    /* The connector's modes into `modes`; its best one in `best_width` and
     * `best_height`, which stay 0 for a connector that lists none. */
    void add_modes(const char *name, DisplayModeSet &modes, uint32_t &best_width, uint32_t &best_height);

    Host &host_;
};

}  // namespace av

#endif  // AV_HOST_MONITOR_MODES_HPP
