#ifndef AV_PRESENT_COMMANDS_HPP
#define AV_PRESENT_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class PresentCopyRequest;

class PresentCommands {
public:
    explicit PresentCommands(Host &host);
    PresentCommands(const PresentCommands &) = delete;
    PresentCommands &operator=(const PresentCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    uint32_t move(ControlRequest &request, const PresentCopyRequest &copy);

    Host &host_;
};

}

#endif
