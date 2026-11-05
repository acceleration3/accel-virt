#ifndef AV_WINDOW_STATS_COMMANDS_HPP
#define AV_WINDOW_STATS_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class WindowStatsWindow;

class WindowStatsCommands {
public:
    WindowStatsCommands(Host &host, WindowStatsWindow &window);
    WindowStatsCommands(const WindowStatsCommands &) = delete;
    WindowStatsCommands &operator=(const WindowStatsCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    void register_page(ControlRequest &request);

    Host              &host_;
    WindowStatsWindow &window_;
};

}

#endif
