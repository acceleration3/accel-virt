#include "device/control/blob_commands.hpp"

#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

BlobCommands::BlobCommands(Host &host, BlobTable &blobs) : host_(host), blobs_(blobs) {}

bool BlobCommands::answer(ControlRequest &request, uint32_t type)
{
    host_.require_main_loop("BlobCommands::answer");
    GpuResponse response;
    switch (type) {
    case resource_create_blob: response.set(create(request)); break;
    case resource_map_blob: map(request, response); break;
    case resource_unmap_blob: response.set(unmap(request)); break;
    case set_scanout_blob: response.set(GpuResponse::err_invalid_parameter); break;
    case resource_unref:
        if (!unref(request, response)) return false;
        break;
    case resource_create_2d:
        if (!blobs_.contains(named_resource(request))) return false;
        response.set(GpuResponse::err_invalid_resource_id);
        break;
    default: return false;
    }
    request.respond(response);
    return true;
}

/* A base-class resource, or a short request the base class refuses, is not ours. */
bool BlobCommands::unref(ControlRequest &request, GpuResponse &response)
{
    switch (blobs_.release(named_resource(request))) {
    case Status::ok: return true;
    case Status::invalid_state: response.set(GpuResponse::err_invalid_parameter); return true;
    default: return false;
    }
}

uint32_t BlobCommands::named_resource(ControlRequest &request)
{
    uint8_t     bytes[32] = {};
    WireReader  reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest named;
    return named.decode_resource(reader) == Status::ok ? named.resource_id() : 0u;
}

uint32_t BlobCommands::create(ControlRequest &request)
{
    uint8_t           bytes[CreateBlobRequest::fixed_size] = {};
    WireReader        reader(bytes, request.read(0u, bytes, sizeof bytes));
    CreateBlobRequest create;
    if (create.decode(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    const uint32_t id = create.resource_id();
    if (id == 0u || host_.resource_exists(id) || blobs_.contains(id)) return GpuResponse::err_invalid_resource_id;
    if (create.blob_id() != no_context_object) return GpuResponse::err_invalid_parameter;
    if (create.memory() == CreateBlobRequest::memory_guest)
        return create_guest(request, id, create.entries(), create.size());
    if (create.memory() == CreateBlobRequest::memory_host3d)
        return create_host(id, create.flags(), create.entries(), create.size());
    return GpuResponse::err_invalid_parameter;
}

uint32_t BlobCommands::create_guest(ControlRequest &request, uint32_t id, uint32_t entries, uint64_t size)
{
    return create_answer(blobs_.create_guest(id, size, request, CreateBlobRequest::fixed_size, entries));
}

/* Device-allocated mappable memory: no memory entries, no context object. */
uint32_t BlobCommands::create_host(uint32_t id, uint32_t flags, uint32_t entries, uint64_t size)
{
    if ((flags & CreateBlobRequest::flag_mappable) == 0u || entries != 0u) return GpuResponse::err_invalid_parameter;
    return create_answer(blobs_.create_host(id, size));
}

uint32_t BlobCommands::create_answer(Status status) const
{
    switch (status) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::invalid_parameter: return GpuResponse::err_invalid_parameter;
    case Status::no_memory:
    case Status::no_space: return GpuResponse::err_out_of_memory;
    default: return GpuResponse::err_unspec;
    }
}

void BlobCommands::map(ControlRequest &request, GpuResponse &response)
{
    uint8_t     bytes[40] = {};
    WireReader  reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest map;
    const Status decoded = map.decode_map(reader);
    switch (decoded == Status::ok ? blobs_.map(map.resource_id(), map.offset()) : Status::invalid_parameter) {
    case Status::ok: response.set_map_info(GpuResponse::map_cache_cached); break;
    case Status::not_found: response.set(GpuResponse::err_invalid_resource_id); break;
    case Status::invalid_parameter: response.set(GpuResponse::err_invalid_parameter); break;
    default: response.set(GpuResponse::err_unspec); break;
    }
}

uint32_t BlobCommands::unmap(ControlRequest &request)
{
    uint8_t     bytes[32] = {};
    WireReader  reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest unmap;
    if (unmap.decode_resource(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    switch (blobs_.unmap(unmap.resource_id())) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::not_found: return GpuResponse::err_invalid_resource_id;
    default: return GpuResponse::err_invalid_parameter;
    }
}

}  // namespace av
