#ifndef AV_SHARED_COMMANDS_HPP
#define AV_SHARED_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ContextPlane;
class ControlRequest;
class ExportService;
class Host;

class SharedCommands {
public:
    SharedCommands(Host &host, ContextPlane &contexts, ExportService &exports, const uint8_t *gpu_uuid);
    SharedCommands(const SharedCommands &) = delete;
    SharedCommands &operator=(const SharedCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    Host          &host_;
    ContextPlane  &contexts_;
    ExportService &exports_;
    const uint8_t *gpu_uuid_;
};

}

#endif
