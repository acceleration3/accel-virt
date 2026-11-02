#include "device/control/display_mode_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/display/display_modes.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

namespace av {

DisplayModeCommands::DisplayModeCommands(Host &host, const DisplayModes &modes) : host_(host), modes_(modes)
{
    AV_SCOPE("DisplayModeCommands::DisplayModeCommands");
}

bool DisplayModeCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("DisplayModeCommands::answer");
    host_.require_main_loop("DisplayModeCommands::answer");
    if (type != DisplayModeSet::request_type) return false;
    GpuResponse response;
    if (modes_.current().count() == 0u) response.set(GpuResponse::err_unspec);
    else response.set_display_modes(modes_.current());
    request.respond(response);
    return true;
}

}
