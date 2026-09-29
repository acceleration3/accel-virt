/* GET_DISPLAY_MODES (docs/protocol.md): the modes the device's display offers,
 * which the driver reads at start and again on each display event. A device
 * with none to give answers ERR_UNSPEC, and the driver keeps the modes
 * GET_DISPLAY_INFO's rectangle allows. Main loop. */
#ifndef AV_DISPLAY_MODE_COMMANDS_HPP
#define AV_DISPLAY_MODE_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class DisplayModes;
class Host;

class DisplayModeCommands {
public:
    DisplayModeCommands(Host &host, const DisplayModes &modes);
    DisplayModeCommands(const DisplayModeCommands &) = delete;
    DisplayModeCommands &operator=(const DisplayModeCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    Host               &host_;
    const DisplayModes &modes_;
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_COMMANDS_HPP
