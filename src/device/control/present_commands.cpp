#include "device/control/present_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/control/region_mover.hpp"
#include "device/host/host.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

static_assert(PresentCopyRequest::max_entries <= GuestPageMapping::max_entries,
              "every page list a request may carry is one the device maps");
static_assert(PresentCopyRequest::entry_size == GuestPageMapping::entry_size, "one memory entry, one layout");

PresentCommands::PresentCommands(Host &host) : host_(host)
{
    AV_SCOPE("PresentCommands::PresentCommands");
}

bool PresentCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("PresentCommands::answer");
    host_.require_main_loop("PresentCommands::answer");
    if (type != PresentCopyRequest::type) return false;
    uint8_t            fixed[PresentCopyRequest::encoded_size] = {};
    WireReader         reader(fixed, request.read(0u, fixed, sizeof fixed));
    PresentCopyRequest copy;
    GpuResponse        response;
    response.set(copy.decode(reader) == Status::ok ? move(request, copy) : GpuResponse::err_invalid_parameter);
    request.respond(response);
    return true;
}

uint32_t PresentCommands::move(ControlRequest &request, const PresentCopyRequest &copy)
{
    AV_SCOPE("PresentCommands::move");
    GuestPageMapping source(host_);
    GuestPageMapping destination(host_);
    Status           mapped = copy.operation() == PresentOperation::blt
                                  ? source.map(request, PresentCopyRequest::encoded_size, copy.source_entries())
                                  : Status::ok;
    if (mapped == Status::ok)
        mapped = destination.map(request, copy.destination_entries_offset(), copy.destination_entries());
    if (mapped == Status::no_memory) return GpuResponse::err_out_of_memory;
    if (mapped != Status::ok) return GpuResponse::err_invalid_parameter;
    RegionMover mover(source.window(), destination.window(), copy);
    for (uint32_t index = 0u; index < copy.regions(); index++) {
        uint8_t       bytes[PresentRegion::encoded_size] = {};
        WireReader    reader(bytes, request.read(copy.regions_offset() + index * PresentRegion::encoded_size, bytes,
                                                 sizeof bytes));
        PresentRegion region;
        if (region.decode(reader) != Status::ok || mover.move(region) != Status::ok)
            return GpuResponse::err_invalid_parameter;
    }
    return GpuResponse::ok_nodata;
}

}  // namespace av
