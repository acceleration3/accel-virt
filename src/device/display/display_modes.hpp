/* The modes this device's display offers the guest: the ones it was told to
 * offer - its `modes` property, set before the VM starts or while it runs - or,
 * told none, the host's own monitors' (HostMonitorModes). A host with neither
 * offers none, and the guest keeps what GET_DISPLAY_INFO's rectangle allows.
 * Main loop. */
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

    /* `text` null or blank: the host's monitors. invalid_parameter for text
     * that is no list of modes, which leaves the modes as they were; not_found
     * for no text and no monitor, which leaves none. */
    [[nodiscard]] Status set(const char *text);
    const DisplayModeSet &current() const;

private:
    Host          &host_;
    DisplayModeSet modes_;
};

}  // namespace av

#endif  // AV_DISPLAY_MODES_HPP
