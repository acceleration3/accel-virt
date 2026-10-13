#include "device/control/scanout_plane_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/display/head_planes.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

ScanoutPlaneCommands::ScanoutPlaneCommands(Host &host, HeadPlanes &planes) : host_(host), planes_(planes)
{
    AV_SCOPE("ScanoutPlaneCommands::ScanoutPlaneCommands");
}

bool ScanoutPlaneCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("ScanoutPlaneCommands::answer");
    host_.require_main_loop("ScanoutPlaneCommands::answer");
    if (type != ScanoutPlaneRequest::type) return false;
    uint8_t             bytes[ScanoutPlaneRequest::encoded_size] = {};
    WireReader          reader(bytes, request.read(0u, bytes, sizeof bytes));
    ScanoutPlaneRequest plane;
    GpuResponse         response;
    const bool kept = plane.decode(reader) == Status::ok && planes_.set(plane) == Status::ok;
    response.set(kept ? GpuResponse::ok_nodata : GpuResponse::err_invalid_parameter);
    request.respond(response);
    return true;
}

}  // namespace av
