#ifndef AV_PRESENT_ENGINE_COMMANDS_HPP
#define AV_PRESENT_ENGINE_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class PresentEngine;
class PresentFrames;

class PresentEngineCommands {
public:
    PresentEngineCommands(Host &host, PresentEngine &engine, const PresentFrames &snapshots);
    PresentEngineCommands(const PresentEngineCommands &) = delete;
    PresentEngineCommands &operator=(const PresentEngineCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    void register_page(ControlRequest &request);

    Host                   &host_;
    PresentEngine          &engine_;
    const PresentFrames &snapshots_;
};

}

#endif
