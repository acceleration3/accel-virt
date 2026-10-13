/* SCANOUT_PLANE (docs/protocol.md, "Scanout planes"): a head's overlay plane,
 * kept for the frames the device composes. */
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

    /* True when `type` was this class's to answer, which it then did. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    Host       &host_;
    HeadPlanes &planes_;
};

}  // namespace av

#endif  // AV_SCANOUT_PLANE_COMMANDS_HPP
