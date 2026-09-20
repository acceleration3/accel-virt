#include "device/control/scanout_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/display/display_heads.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_table.hpp"
#include "proto/virtio_gpu/flush_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

ScanoutCommands::ScanoutCommands(Host &host, BlobTable &blobs, DisplayHeads &heads)
    : host_(host), blobs_(blobs), heads_(heads)
{
    AV_SCOPE("ScanoutCommands::ScanoutCommands");
}

bool ScanoutCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("ScanoutCommands::answer");
    host_.require_main_loop("ScanoutCommands::answer");
    GpuResponse response;
    switch (type) {
    case set_scanout_blob: response.set(show(request)); break;
    case resource_flush: {
        uint32_t flushed = 0u;
        if (!flush(request, flushed)) return false;
        response.set(flushed);
        break;
    }
    default: return false;
    }
    request.respond(response);
    return true;
}

uint32_t ScanoutCommands::show(ControlRequest &request)
{
    AV_SCOPE("ScanoutCommands::show");
    uint8_t               bytes[SetScanoutBlobRequest::encoded_size] = {};
    WireReader            reader(bytes, request.read(0u, bytes, sizeof bytes));
    SetScanoutBlobRequest shown;
    if (shown.decode(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    if (shown.resource_id() == 0u)
        return heads_.hide(shown.scanout_id()) == Status::ok ? GpuResponse::ok_nodata
                                                             : GpuResponse::err_invalid_parameter;
    uint64_t     blob_size = 0u;
    if (blobs_.size(shown.resource_id(), blob_size) != Status::ok) return GpuResponse::err_invalid_resource_id;
    ScanoutFramebuffer framebuffer;
    if (framebuffer.adopt(shown, blob_size) != Status::ok) return GpuResponse::err_invalid_parameter;
    return shown_answer(heads_.show(shown.scanout_id(), shown.resource_id(), framebuffer));
}

/* A refusal that is the guest's own is invalid_parameter; everything else is
 * the host declining to show the descriptor, which is not the guest's fault
 * and not a thing it can fix. */
uint32_t ScanoutCommands::shown_answer(Status status) const
{
    AV_SCOPE("ScanoutCommands::shown_answer");
    switch (status) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::invalid_parameter: return GpuResponse::err_invalid_parameter;
    default: return GpuResponse::err_unspec;
    }
}

bool ScanoutCommands::flush(ControlRequest &request, uint32_t &response)
{
    AV_SCOPE("ScanoutCommands::flush");
    uint8_t      bytes[FlushRequest::encoded_size] = {};
    WireReader   reader(bytes, request.read(0u, bytes, sizeof bytes));
    FlushRequest flushed;
    /* A malformed flush is the base class's to refuse, as it always was; only
     * a well-formed one naming a blob is ours. */
    if (flushed.decode(reader) != Status::ok) return false;
    if (!blobs_.contains(flushed.resource_id())) return false;
    response = heads_.flush(flushed.resource_id(), flushed.rect()) == Status::ok
                   ? GpuResponse::ok_nodata
                   : GpuResponse::err_invalid_parameter;
    return true;
}

}  // namespace av
