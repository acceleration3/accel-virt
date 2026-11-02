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

    bool answer(ControlRequest &request, uint32_t type);

private:
    Host               &host_;
    const DisplayModes &modes_;
};

}

#endif
