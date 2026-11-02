#ifndef AV_DISPLAY_CLOCK_COMMANDS_HPP
#define AV_DISPLAY_CLOCK_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class DisplayClock;
class Host;

class DisplayClockCommands {
public:
    DisplayClockCommands(Host &host, DisplayClock &clock);
    DisplayClockCommands(const DisplayClockCommands &) = delete;
    DisplayClockCommands &operator=(const DisplayClockCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    void register_page(ControlRequest &request);
    void retime(ControlRequest &request);

    Host         &host_;
    DisplayClock &clock_;
};

}

#endif
