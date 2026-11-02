#include "device/control/shared_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"
#include "device/control/control_request.hpp"
#include "device/export/export_service.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

SharedCommands::SharedCommands(Host &host, ContextPlane &contexts, ExportService &exports, const uint8_t *gpu_uuid)
    : host_(host), contexts_(contexts), exports_(exports), gpu_uuid_(gpu_uuid)
{
    AV_SCOPE("SharedCommands::SharedCommands");
}

bool SharedCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("SharedCommands::answer");
    host_.require_main_loop("SharedCommands::answer");
    if (type != SharedReleaseRequest::type) return false;
    uint8_t              bytes[SharedReleaseRequest::encoded_size] = {};
    WireReader           reader(bytes, request.read(0u, bytes, sizeof bytes));
    SharedReleaseRequest release;
    GpuResponse          response;
    if (release.decode(reader) != Status::ok) {
        response.set(GpuResponse::err_invalid_parameter);
    } else {
        contexts_.shared_released(gpu_uuid_, release.object_id());
        exports_.revoke(release.object_id(), ExportRevocation::released);
        response.set(GpuResponse::ok_nodata);
    }
    request.respond(response);
    return true;
}

}
