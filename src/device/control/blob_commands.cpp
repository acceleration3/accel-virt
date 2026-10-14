#include "device/control/blob_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/abi/av_executor.h"
#include "proto/resources/host_memory_request.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

BlobCommands::BlobCommands(Host &host, BlobTable &blobs, HostBlobTable &host_blobs, WireRecorder &recorder,
                           ContextPlane &contexts)
    : host_(host), blobs_(blobs), host_blobs_(host_blobs), recorder_(recorder), contexts_(contexts)
{
    AV_SCOPE("BlobCommands::BlobCommands");
}

bool BlobCommands::answer(ControlRequest &request, uint32_t type)
{
    AV_SCOPE("BlobCommands::answer");
    host_.require_main_loop("BlobCommands::answer");
    GpuResponse response;
    switch (type) {
    case resource_create_blob: response.set(create(request)); break;
    case resource_map_blob: map(request, response); break;
    case resource_unmap_blob: response.set(unmap(request)); break;
    case resource_unref:
        if (!unref(request, response)) return false;
        break;
    case resource_create_2d:
        if (!blobs_.contains(named_resource(request)) && !host_blobs_.contains(named_resource(request))) return false;
        response.set(GpuResponse::err_invalid_resource_id);
        break;
    default: return false;
    }
    request.respond(response);
    return true;
}

/* A base-class resource, or a short request the base class refuses, is not
 * ours. A guest blob still pinned is refused, not released; a host blob is
 * always released, since an import holding its memory keeps it in the
 * library rather than here. */
bool BlobCommands::unref(ControlRequest &request, GpuResponse &response)
{
    AV_SCOPE("BlobCommands::unref");
    const uint32_t resource = named_resource(request);
    if (host_blobs_.release(resource) == Status::ok) {
        recorder_.record_blob(0u, BlobAction::released, 0u, resource, 0u, 0u);
        return true;
    }
    switch (blobs_.release(resource)) {
    case Status::ok:
        recorder_.record_blob(0u, BlobAction::released, 0u, resource, 0u, 0u);
        contexts_.blob_released(resource);
        return true;
    case Status::invalid_state: response.set(GpuResponse::err_invalid_parameter); return true;
    default: return false;
    }
}

uint32_t BlobCommands::named_resource(ControlRequest &request)
{
    AV_SCOPE("BlobCommands::named_resource");
    uint8_t     bytes[32] = {};
    WireReader  reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest named;
    return named.decode_resource(reader) == Status::ok ? named.resource_id() : 0u;
}

uint32_t BlobCommands::create(ControlRequest &request)
{
    AV_SCOPE("BlobCommands::create");
    uint8_t           bytes[CreateBlobRequest::fixed_size] = {};
    WireReader        reader(bytes, request.read(0u, bytes, sizeof bytes));
    CreateBlobRequest create;
    if (create.decode(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    const uint32_t id = create.resource_id();
    if (id == 0u || host_.resource_exists(id) || blobs_.contains(id) || host_blobs_.contains(id))
        return GpuResponse::err_invalid_resource_id;
    if (create.memory() == CreateBlobRequest::memory_host3d) return create_host(create);
    if (create.blob_id() != no_context_object) return GpuResponse::err_invalid_parameter;
    if (create.memory() == CreateBlobRequest::memory_guest)
        return create_guest(request, id, create.entries(), create.size());
    return GpuResponse::err_invalid_parameter;
}

uint32_t BlobCommands::create_guest(ControlRequest &request, uint32_t id, uint32_t entries, uint64_t size)
{
    AV_SCOPE("BlobCommands::create_guest");
    const uint32_t answer = create_answer(blobs_.create_guest(id, size, request, CreateBlobRequest::fixed_size, entries));
    if (answer != GpuResponse::ok_nodata) return answer;
    recorder_.record_blob(0u, BlobAction::created, 0u, id, 0u, size);
    contexts_.blob_created(id);
    return answer;
}

/* Host GPU memory for one live context: mappable, no memory entries, and
 * the memory it is in named as the context object. */
uint32_t BlobCommands::create_host(const CreateBlobRequest &create)
{
    AV_SCOPE("BlobCommands::create_host");
    if ((create.flags() & CreateBlobRequest::flag_mappable) == 0u || create.entries() != 0u ||
        (create.blob_id() & ~request_mask) != 0u)
        return GpuResponse::err_invalid_parameter;
    if (!contexts_.holds(create.context_id())) return GpuResponse::err_invalid_context_id;
    const HostMemoryRequest memory(static_cast<uint32_t>(create.blob_id()));
    switch (host_blobs_.create(create.resource_id(), create.context_id(), memory, create.size())) {
    case Status::ok: break;
    case Status::not_found: return GpuResponse::err_invalid_resource_id;
    case Status::no_memory: return GpuResponse::err_out_of_memory;
    default: return GpuResponse::err_invalid_parameter;
    }
    recorder_.record_blob(create.context_id(), BlobAction::created, 0u, create.resource_id(), 0u, create.size());
    return GpuResponse::ok_nodata;
}

uint32_t BlobCommands::create_answer(Status status) const
{
    AV_SCOPE("BlobCommands::create_answer");
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
    AV_SCOPE("BlobCommands::map");
    uint8_t      bytes[40] = {};
    WireReader   reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest  map;
    uint32_t     caching = 0u;
    const Status decoded = map.decode_map(reader);
    switch (decoded == Status::ok ? host_blobs_.map(map.resource_id(), map.offset(), caching)
                                  : Status::invalid_parameter) {
    case Status::ok:
        response.set_map_info(caching == AV_MEMORY_WRITE_BACK ? GpuResponse::map_cache_cached
                                                              : GpuResponse::map_cache_wc);
        break;
    case Status::not_found: response.set(GpuResponse::err_invalid_resource_id); break;
    case Status::invalid_parameter: response.set(GpuResponse::err_invalid_parameter); break;
    default: response.set(GpuResponse::err_unspec); break;
    }
}

uint32_t BlobCommands::unmap(ControlRequest &request)
{
    AV_SCOPE("BlobCommands::unmap");
    uint8_t     bytes[32] = {};
    WireReader  reader(bytes, request.read(0u, bytes, sizeof bytes));
    BlobRequest unmap;
    if (unmap.decode_resource(reader) != Status::ok) return GpuResponse::err_invalid_parameter;
    switch (host_blobs_.unmap(unmap.resource_id())) {
    case Status::ok: return GpuResponse::ok_nodata;
    case Status::not_found: return GpuResponse::err_invalid_resource_id;
    default: return GpuResponse::err_invalid_parameter;
    }
}

}  // namespace av
