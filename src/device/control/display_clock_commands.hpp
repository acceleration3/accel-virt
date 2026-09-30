/* DISPLAY_CLOCK and DISPLAY_TIMING (docs/protocol.md, "Display clock"): where
 * the device publishes its vertical blanks, and how often each head has one.
 * Main loop. */
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

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    void register_page(ControlRequest &request);
    void retime(ControlRequest &request);

    Host         &host_;
    DisplayClock &clock_;
};

}  // namespace av

#endif  // AV_DISPLAY_CLOCK_COMMANDS_HPP
