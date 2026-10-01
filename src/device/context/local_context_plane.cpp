#include "device/context/local_context_plane.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

LocalContextPlane::LocalContextPlane(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, WireRecorder &recorder)
    : library_(), shared_(host), contexts_(host, blobs, shared_, interrupts, library_, recorder)
{
    AV_SCOPE("LocalContextPlane::LocalContextPlane");
}

LocalContextPlane::~LocalContextPlane()
{
    AV_SCOPE("LocalContextPlane::~LocalContextPlane");
}

Status LocalContextPlane::accept(const AvExecutorOps *executor)
{
    AV_SCOPE("LocalContextPlane::accept");
    return library_.accept(executor);
}

/* A realize that failed before may already hold the locks: only no_memory is
 * a failure. */
Status LocalContextPlane::initialize()
{
    AV_SCOPE("LocalContextPlane::initialize");
    if (shared_.initialize() == Status::no_memory) return Status::no_memory;
    return contexts_.initialize() == Status::no_memory ? Status::no_memory : Status::ok;
}

/* The library was loaded with QEMU and runs in it: nothing to start. */
Status LocalContextPlane::start(const uint8_t *, const CapsetInfo &)
{
    AV_SCOPE("LocalContextPlane::start");
    return Status::ok;
}

/* The contexts read QEMU's own blob table and raise QEMU's own interrupts. */
void LocalContextPlane::blob_created(uint32_t)
{
    AV_SCOPE("LocalContextPlane::blob_created");
}

void LocalContextPlane::blob_released(uint32_t)
{
    AV_SCOPE("LocalContextPlane::blob_released");
}

/* Object 0 is every one: a reset, after which the guest's ids start again. */
void LocalContextPlane::shared_released(const uint8_t *gpu_uuid, uint64_t object_id)
{
    AV_SCOPE("LocalContextPlane::shared_released");
    if (object_id == 0u) shared_.release_all();
    else shared_.close(object_id);
    library_.release_shared(gpu_uuid, object_id);
}

void LocalContextPlane::interrupts_changed(bool)
{
    AV_SCOPE("LocalContextPlane::interrupts_changed");
}

void LocalContextPlane::renderer_lost()
{
    AV_SCOPE("LocalContextPlane::renderer_lost");
}

uint32_t LocalContextPlane::sub_apis() const
{
    AV_SCOPE("LocalContextPlane::sub_apis");
    return library_.sub_apis();
}

uint32_t LocalContextPlane::import_kinds(SubApi api, const uint8_t *gpu_uuid) const
{
    AV_SCOPE("LocalContextPlane::import_kinds");
    return library_.import_kinds(api, gpu_uuid);
}

Status LocalContextPlane::create(uint32_t id)
{
    AV_SCOPE("LocalContextPlane::create");
    return contexts_.create(id);
}

Status LocalContextPlane::destroy(uint32_t id)
{
    AV_SCOPE("LocalContextPlane::destroy");
    return contexts_.destroy(id);
}

/* The contexts drop their references as they go; whatever is left belongs to
 * no context and goes too. */
void LocalContextPlane::destroy_all()
{
    AV_SCOPE("LocalContextPlane::destroy_all");
    contexts_.destroy_all();
    shared_.release_all();
}

bool LocalContextPlane::holds(uint32_t id)
{
    AV_SCOPE("LocalContextPlane::holds");
    return contexts_.find(id) != nullptr;
}

Status LocalContextPlane::bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset)
{
    AV_SCOPE("LocalContextPlane::bind");
    DeviceContext *const context = contexts_.find(id);
    if (context == nullptr) return Status::invalid_parameter;
    return context->bind(setup, capset);
}

bool LocalContextPlane::holds_doorbell(uint32_t index)
{
    AV_SCOPE("LocalContextPlane::holds_doorbell");
    return contexts_.holds_doorbell(index);
}

bool LocalContextPlane::can_share(uint32_t asking, uint32_t share)
{
    AV_SCOPE("LocalContextPlane::can_share");
    return contexts_.can_share(asking, share);
}

uint32_t LocalContextPlane::live() const
{
    AV_SCOPE("LocalContextPlane::live");
    return contexts_.live();
}

bool LocalContextPlane::export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("LocalContextPlane::export_image");
    contexts_.settle_d3d();
    return library_.export_image(gpu_uuid, key, image);
}

bool LocalContextPlane::read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride,
                                   uint32_t width, uint32_t height)
{
    AV_SCOPE("LocalContextPlane::read_image");
    contexts_.settle_d3d();
    return library_.read_image(gpu_uuid, key, pixels, stride, width, height);
}

bool LocalContextPlane::reads_frames() const
{
    AV_SCOPE("LocalContextPlane::reads_frames");
    return library_.reads_frames();
}

bool LocalContextPlane::map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("LocalContextPlane::map_frame");
    return library_.map_frame(gpu_uuid, frame, wait_ms, view);
}

void LocalContextPlane::unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame)
{
    AV_SCOPE("LocalContextPlane::unmap_frame");
    library_.unmap_frame(gpu_uuid, frame);
}

void LocalContextPlane::report(StateWriter &writer)
{
    AV_SCOPE("LocalContextPlane::report");
    shared_.report(writer);
    contexts_.report(writer);
}

bool LocalContextPlane::invariants_hold() const
{
    AV_SCOPE("LocalContextPlane::invariants_hold");
    return contexts_.invariants_hold() && shared_.invariants_hold();
}

}  // namespace av
