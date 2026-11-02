#ifndef AV_SCANOUT_PLANE_COMMANDS_HPP
#define AV_SCANOUT_PLANE_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class HeadPlanes;
class Host;

class ScanoutPlaneCommands {
public:
    ScanoutPlaneCommands(Host &host, HeadPlanes &planes);

    bool answer(ControlRequest &request, uint32_t type);

private:
    Host       &host_;
    HeadPlanes &planes_;
};

}

#endif
