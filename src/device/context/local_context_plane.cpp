#include "device/context/local_context_plane.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

LocalContextPlane::LocalContextPlane(Host &host, BlobTable &blobs, HostBlobOwners &owners, FenceInterrupts &interrupts,
                                     WireRecorder &recorder)
    : library_(), contexts_(host, blobs, owners, interrupts, library_, recorder)
{
    AV_SCOPE("LocalContextPlane::LocalContextPlane");
}

/* The contexts first: none may share in a domain that is closing. */
LocalContextPlane::~LocalContextPlane()
{
    AV_SCOPE("LocalContextPlane::~LocalContextPlane");
    contexts_.destroy_all();
    library_.close_domain();
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
    return contexts_.initialize() == Status::no_memory ? Status::no_memory : Status::ok;
}

/* The library was loaded with QEMU and runs in it: all there is to start is
 * this device's domain in it. A GPU the library has not got is a device whose
 * every context is refused, as one whose capset offers nothing is, not one
 * that fails to realize; a device restarted keeps the domain it has. */
Status LocalContextPlane::start(const uint8_t *gpu_uuid, const CapsetInfo &)
{
    AV_SCOPE("LocalContextPlane::start");
    if (library_.domain() == 0u) static_cast<void>(library_.open_domain(gpu_uuid));
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
void LocalContextPlane::shared_released(const uint8_t *, uint64_t object_id)
{
    AV_SCOPE("LocalContextPlane::shared_released");
    library_.release_shared(object_id);
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

Status LocalContextPlane::allocate_memory(uint32_t, uint64_t key, uint64_t size, uint32_t request,
                                          AvHostMemory &memory)
{
    AV_SCOPE("LocalContextPlane::allocate_memory");
    return library_.allocate_memory(key, size, request, memory);
}

void LocalContextPlane::free_memory(uint64_t key)
{
    AV_SCOPE("LocalContextPlane::free_memory");
    library_.free_memory(key);
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

void LocalContextPlane::destroy_all()
{
    AV_SCOPE("LocalContextPlane::destroy_all");
    contexts_.destroy_all();
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

/* The device's own domain answers for every GPU named here: one device is
 * on one GPU. */
bool LocalContextPlane::export_image(const uint8_t *, uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("LocalContextPlane::export_image");
    contexts_.settle_d3d();
    return library_.export_image(key, image);
}

bool LocalContextPlane::composes() const
{
    AV_SCOPE("LocalContextPlane::composes");
    return library_.composes();
}

bool LocalContextPlane::compose_planes(const uint8_t *, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                       uint32_t height, AvExportedImage &image)
{
    AV_SCOPE("LocalContextPlane::compose_planes");
    return library_.compose_planes(planes, count, width, height, image);
}

bool LocalContextPlane::read_image(const uint8_t *, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                                   uint32_t height)
{
    AV_SCOPE("LocalContextPlane::read_image");
    contexts_.settle_d3d();
    return library_.read_image(key, pixels, stride, width, height);
}

bool LocalContextPlane::reads_frames() const
{
    AV_SCOPE("LocalContextPlane::reads_frames");
    return library_.available();
}

bool LocalContextPlane::map_frame(const uint8_t *, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("LocalContextPlane::map_frame");
    return library_.map_frame(frame, wait_ms, view);
}

void LocalContextPlane::unmap_frame(const uint8_t *, const SurfacePoint &frame)
{
    AV_SCOPE("LocalContextPlane::unmap_frame");
    library_.unmap_frame(frame);
}

bool LocalContextPlane::exports_surfaces() const
{
    AV_SCOPE("LocalContextPlane::exports_surfaces");
    return library_.available();
}

bool LocalContextPlane::export_surface(const uint8_t *, uint64_t surface, AvExportedImage &image)
{
    AV_SCOPE("LocalContextPlane::export_surface");
    return library_.export_surface(surface, image);
}

bool LocalContextPlane::sync_surface(const uint8_t *, uint64_t surface, uint64_t &writes, int &fd)
{
    AV_SCOPE("LocalContextPlane::sync_surface");
    return library_.sync_surface(surface, writes, fd);
}

void LocalContextPlane::report(StateWriter &writer)
{
    AV_SCOPE("LocalContextPlane::report");
    contexts_.report(writer);
}

bool LocalContextPlane::invariants_hold() const
{
    AV_SCOPE("LocalContextPlane::invariants_hold");
    return contexts_.invariants_hold();
}

}  // namespace av
