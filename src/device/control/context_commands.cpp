#include "device/control/context_commands.hpp"

#include "device/capset/device_capset.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/virtio_gpu/capset_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

ContextCommands::ContextCommands(Host &host, DeviceContextTable &contexts, const DeviceCapset &capset)
    : host_(host), contexts_(contexts), capset_(capset) {}

bool ContextCommands::answer(ControlRequest &request, uint32_t type)
{
    host_.require_main_loop("ContextCommands::answer");
    GpuResponse response;
    switch (type) {
    case get_capset_info: capset_info(request, response); break;
    case get_capset: capset(request, response); break;
    case ctx_create: response.set(create(request)); break;
    case ctx_destroy: response.set(destroy(request)); break;
    case submit_3d: response.set(submit(request)); break;
    default: return false;
    }
    request.respond(response);
    return true;
}

/* One capset, at index 0. */
void ContextCommands::capset_info(ControlRequest &request, GpuResponse &response)
{
    uint8_t       bytes[CapsetRequest::encoded_size] = {};
    WireReader    reader(bytes, request.read(0u, bytes, sizeof bytes));
    CapsetRequest info;
    if (!capset_.described() || info.decode_info(reader) != Status::ok || info.index() != 0u) {
        response.set(GpuResponse::err_invalid_parameter);
        return;
    }
    response.set_capset_info(CapsetInfo::capset_id, DeviceCapset::capset_version, CapsetInfo::encoded_size);
}

void ContextCommands::capset(ControlRequest &request, GpuResponse &response)
{
    uint8_t       bytes[CapsetRequest::encoded_size] = {};
    WireReader    reader(bytes, request.read(0u, bytes, sizeof bytes));
    CapsetRequest wanted;
    if (!capset_.described() || wanted.decode_capset(reader) != Status::ok ||
        wanted.capset_id() != CapsetInfo::capset_id ||
        wanted.capset_version() != DeviceCapset::capset_version) {
        response.set(GpuResponse::err_invalid_parameter);
        return;
    }
    response.set_capset(capset_.info());
}

/* Only the rvk capset: context_init's reserved bits must be clear too. */
uint32_t ContextCommands::create(ControlRequest &request)
{
    uint8_t        bytes[ContextRequest::create_size] = {};
    WireReader     reader(bytes, request.read(0u, bytes, sizeof bytes));
    ContextRequest create;
    if (!capset_.described() || create.decode_create(reader) != Status::ok ||
        create.context_init() != CapsetInfo::capset_id ||
        create.name_length() > ContextRequest::name_capacity)
        return GpuResponse::err_invalid_parameter;
    switch (contexts_.create(create.context_id())) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::invalid_parameter:
    case Status::conflict: return GpuResponse::err_invalid_context_id;
    default: return GpuResponse::err_out_of_memory;
    }
}

uint32_t ContextCommands::destroy(ControlRequest &request)
{
    uint8_t        bytes[ContextRequest::header_size] = {};
    WireReader     reader(bytes, request.read(0u, bytes, sizeof bytes));
    ContextRequest destroy;
    if (destroy.decode_destroy(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    return contexts_.destroy(destroy.context_id()) == Status::ok ? GpuResponse::ok_nodata
                                                                 : GpuResponse::err_invalid_context_id;
}

/* The command buffer is exactly one context setup. */
uint32_t ContextCommands::submit(ControlRequest &request)
{
    uint8_t        bytes[ContextRequest::submit_size] = {};
    WireReader     reader(bytes, request.read(0u, bytes, sizeof bytes));
    ContextRequest submit;
    if (submit.decode_submit(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    DeviceContext *const context = contexts_.find(submit.context_id());
    if (context == nullptr) return GpuResponse::err_invalid_context_id;
    uint8_t      command[ContextSetup::encoded_size] = {};
    ContextSetup setup;
    if (submit.command_size() != ContextSetup::encoded_size ||
        setup.decode(command, request.read(ContextRequest::submit_size, command, sizeof command)) != Status::ok ||
        contexts_.holds_doorbell(setup.doorbell()))
        return GpuResponse::err_invalid_parameter;
    switch (context->bind(setup, capset_.info())) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::not_found: return GpuResponse::err_invalid_resource_id;
    default: return GpuResponse::err_invalid_parameter;
    }
}

}  // namespace av
