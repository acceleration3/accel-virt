#include "executor/executor_root.hpp"

#include "executor/executor_log.hpp"

#include "executor/shim/d3d11_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/host_apis.hpp"
#include "executor/host_gpu.hpp"
#include "executor/host_memory_source.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/sharing/host_shares.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

namespace {

/* One decode block per session, in the library's own storage rather than in
 * the root: sixty four megabytes is not something to carry inside the object
 * that owns the sessions, and nothing but this file ever names it. */
alignas(16) uint8_t decode_blocks[ExecutorRoot::sessions][ContextSession::arena_bytes];

}  // namespace

ExecutorRoot::ExecutorRoot(ExecutorLog &log)
    : gpus_(), domains_(), sessions_{}, vulkan_{}, opencl_{}, opengl_{}, d3d_{}, shares_{}, apis_of_{}, gpus_of_{},
      apis_(nullptr)
{
    AV_SCOPE("ExecutorRoot::ExecutorRoot");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        static_cast<void>(sessions_[slot].construct(log.sessions()));
        static_cast<void>(vulkan_[slot].construct(log.vulkan()));
        static_cast<void>(opencl_[slot].construct(log.opencl()));
        static_cast<void>(opengl_[slot].construct(log.opengl()));
        static_cast<void>(d3d_[slot].construct(log));
    }
}

ExecutorRoot::~ExecutorRoot()
{
    AV_SCOPE("ExecutorRoot::~ExecutorRoot");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        d3d_[slot].destroy();
        opengl_[slot].destroy();
        opencl_[slot].destroy();
        vulkan_[slot].destroy();
        sessions_[slot].destroy();
    }
}

Status ExecutorRoot::initialize(HostApis &apis)
{
    AV_SCOPE("ExecutorRoot::initialize");
    if (apis_ != nullptr) return Status::invalid_state;
    /* A host with more GPUs than the set holds is still one this serves, with
     * the ones it kept. */
    gpus_.initialize(apis);
    apis_ = &apis;
    return Status::ok;
}

uint32_t ExecutorRoot::sub_apis() const
{
    AV_SCOPE("ExecutorRoot::sub_apis");
    return apis_ != nullptr ? gpus_.sub_apis() : 0u;
}

uint32_t ExecutorRoot::open_domain(const uint8_t *gpu_uuid)
{
    AV_SCOPE("ExecutorRoot::open_domain");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    return gpu != nullptr ? domains_.open(gpu->index()) : 0u;
}

void ExecutorRoot::close_domain(uint32_t domain)
{
    AV_SCOPE("ExecutorRoot::close_domain");
    release_shared(domain, 0u);
    HostMemorySource *const source = source_of(domain);
    if (source != nullptr) source->free_domain(domain);
    uint32_t gpu = 0u;
    static_cast<void>(domains_.close(domain, gpu));
}

HostMemorySource *ExecutorRoot::source_of(uint32_t domain) const
{
    AV_SCOPE("ExecutorRoot::source_of");
    const HostGpu *const gpu = gpu_of(domain);
    return gpu != nullptr ? apis_->memory_source(gpu->index()) : nullptr;
}

Status ExecutorRoot::allocate_memory(uint32_t domain, uint64_t key, uint64_t size, uint32_t request,
                                     AvHostMemory &memory)
{
    AV_SCOPE("ExecutorRoot::allocate_memory");
    HostMemorySource *const source = source_of(domain);
    if (source == nullptr) return Status::no_memory;
    return source->allocate(domain, key, size, HostMemoryRequest(request), memory);
}

Status ExecutorRoot::alias_memory(uint32_t domain, uint64_t key, uint32_t from_domain, uint64_t from_key,
                                  AvHostMemory &memory)
{
    AV_SCOPE("ExecutorRoot::alias_memory");
    HostMemorySource *const source = source_of(domain);
    HostMemorySource *const lender = source_of(from_domain);
    if (source == nullptr || lender == nullptr) return Status::no_memory;
    return source->alias(domain, key, *lender, from_domain, from_key, memory);
}

void ExecutorRoot::free_memory(uint32_t domain, uint64_t key)
{
    AV_SCOPE("ExecutorRoot::free_memory");
    HostMemorySource *const source = source_of(domain);
    if (source != nullptr) source->free(domain, key);
}

bool ExecutorRoot::export_image(uint32_t domain, uint64_t id, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::export_image");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->export_image(domain, id, image);
}

bool ExecutorRoot::compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                  uint32_t height, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::compose_planes");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->compose_planes(domain, planes, count, width, height, image);
}

void ExecutorRoot::release_shared(uint32_t domain, uint64_t id)
{
    AV_SCOPE("ExecutorRoot::release_shared");
    const HostGpu *const gpu = gpu_of(domain);
    if (gpu == nullptr) return;
    /* A GPU the host has has both: they are null only past its count. */
    apis_->shares(gpu->index())->release(domain, id);
    apis_->surfaces(gpu->index())->release(id);
    retire(gpu->index());
}

bool ExecutorRoot::read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width,
                              uint32_t height)
{
    AV_SCOPE("ExecutorRoot::read_image");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->read_image(domain, id, pixels, stride, width, height);
}

bool ExecutorRoot::map_frame(uint32_t domain, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("ExecutorRoot::map_frame");
    const HostGpu *const  gpu = gpu_of(domain);
    SharedSurfaces *const surfaces = gpu != nullptr ? apis_->surfaces(gpu->index()) : nullptr;
    return surfaces != nullptr && surfaces->map(frame, wait_ms, view);
}

void ExecutorRoot::unmap_frame(uint32_t domain, const SurfacePoint &frame)
{
    AV_SCOPE("ExecutorRoot::unmap_frame");
    const HostGpu *const  gpu = gpu_of(domain);
    SharedSurfaces *const surfaces = gpu != nullptr ? apis_->surfaces(gpu->index()) : nullptr;
    if (surfaces != nullptr) surfaces->unmap(frame);
}

StagedFrame::Copying ExecutorRoot::copy_frame(uint32_t domain, const SurfacePoint &frame, uint32_t wait_ms,
                                              const AvFrameCopy &copy)
{
    AV_SCOPE("ExecutorRoot::copy_frame");
    const HostGpu *const  gpu = gpu_of(domain);
    SharedSurfaces *const surfaces = gpu != nullptr ? apis_->surfaces(gpu->index()) : nullptr;
    return surfaces != nullptr ? surfaces->copy(frame, wait_ms, copy) : StagedFrame::Copying::declined;
}

bool ExecutorRoot::export_surface(uint32_t domain, uint64_t surface, uint32_t flags, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::export_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->export_surface(domain, surface, flags, image);
}

bool ExecutorRoot::sync_surface(uint32_t domain, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd)
{
    AV_SCOPE("ExecutorRoot::sync_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->sync_surface(domain, surface, flags, writes, fd);
}

ContextSession *ExecutorRoot::open(uint32_t sub_api, const AvContextOptions &options, ContextSession *share)
{
    AV_SCOPE("ExecutorRoot::open");
    const HostGpu *const gpu = gpu_of(options.domain);
    if (gpu == nullptr) return nullptr;
    const bool options_asked = options.major != 0u || options.minor != 0u || options.profile != 0u || options.flags != 0u;
    if (options_asked && sub_api != AV_EXECUTOR_SUB_API_OPENGL) return nullptr;
    /* An OpenGL context shares another's lists; an OpenCL one shares a GL
     * context's objects (cl_khr_gl_sharing); nothing else shares anything. */
    if (share != nullptr && sub_api != AV_EXECUTOR_SUB_API_OPENGL && sub_api != AV_EXECUTOR_SUB_API_OPENCL)
        return nullptr;
    const OpenGlBackend *const shared = share != nullptr ? sharing(share, *gpu) : nullptr;
    if (share != nullptr && shared == nullptr) return nullptr;
    /* A sub-API this GPU does not run is a context the guest should never have
     * been allowed to ask for, but the capset is the union across GPUs, so it
     * could have been. */
    if ((gpu->sub_apis() & sub_api) == 0u) return nullptr;

    SessionBackend *const kind = backend_in(0u, sub_api);
    /* A value that names no single sub-API - two bits at once - passed the GPU
     * check above and has no backend. */
    if (kind == nullptr) return nullptr;

    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        /* A slot in use refuses to be bound again, which is the test for in use
         * rather than a second one beside it. */
        SessionBackend &backend = *backend_in(slot, sub_api);
        if (sessions_[slot].object().initialize(backend, decode_blocks[slot], ContextSession::arena_bytes) != Status::ok)
            continue;
        shares_[slot].bind(*apis_->shares(gpu->index()), options.domain, backend);
        /* Opened here, at the context's setup, on what the host found when it
         * was asked: the guest's first batch waits behind this, inside
         * dxgkrnl's timeout, so it may cost a context and never a driver
         * start-up (feature 044, T035). A GPU the capset said runs the API can
         * still refuse a context, and then the slot goes back. */
        if (!bind(slot, sub_api, *gpu, options, shared)) {
            shares_[slot].end();
            sessions_[slot].object().release();
            return nullptr;
        }
        apis_of_[slot] = sub_api;
        gpus_of_[slot] = gpu->index();
        HostMemorySource *const source = apis_->memory_source(gpu->index());
        if (source != nullptr) sessions_[slot].object().draw_from(*source, options.domain);
        /* Not OpenCL's: its kernels saturate the GPU between the guest's
         * waits, and the peak state's fixed clock is below what a saturated
         * GPU boosts to - held, the OpenCL samples lost 5 to 10%. */
        if (sub_api != AV_EXECUTOR_SUB_API_OPENCL && clocks_[gpu->index()].open(apis_->render_minor(gpu->index())))
            sessions_[slot].object().hold_clocks(clocks_[gpu->index()]);
        return &sessions_[slot].object();
    }
    return nullptr;
}

SessionBackend *ExecutorRoot::backend_in(uint32_t slot, uint32_t sub_api)
{
    AV_SCOPE("ExecutorRoot::backend_in");
    switch (sub_api) {
    case AV_EXECUTOR_SUB_API_VULKAN: return &vulkan_[slot].object();
    case AV_EXECUTOR_SUB_API_OPENCL: return &opencl_[slot].object();
    case AV_EXECUTOR_SUB_API_OPENGL: return &opengl_[slot].object();
    case AV_EXECUTOR_SUB_API_D3D: return &d3d_[slot].object();
    default: return nullptr;
    }
}

const OpenGlBackend *ExecutorRoot::sharing(const ContextSession *share, const HostGpu &gpu) const
{
    AV_SCOPE("ExecutorRoot::sharing");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot].object() != share) continue;
        const bool fits = share->live() && apis_of_[slot] == AV_EXECUTOR_SUB_API_OPENGL &&
                          gpus_of_[slot] == gpu.index();
        return fits ? &opengl_[slot].object() : nullptr;
    }
    return nullptr;
}

bool ExecutorRoot::bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const AvContextOptions &options,
                        const OpenGlBackend *share)
{
    AV_SCOPE("ExecutorRoot::bind");
    switch (sub_api) {
    /* The GPU's EGL display goes with its OpenCL device: on a host whose
     * OpenCL cannot take a descriptor, OpenGL takes it and OpenCL names the
     * GL object (feature 053, T019). A host with no display shares nothing
     * and runs everything else. */
    case AV_EXECUTOR_SUB_API_OPENCL:
        return opencl_[slot].object().initialize(apis_->opencl_device(gpu.index()), apis_->gl_display(gpu.index()),
                                                 share, sessions_[slot].object().imports());
    case AV_EXECUTOR_SUB_API_OPENGL:
        return opengl_[slot].object().initialize(apis_->gl_display(gpu.index()), &options, share);
    case AV_EXECUTOR_SUB_API_D3D: {
        /* A Direct3D 11 device the sessions before lost is made again for
         * this one once they are gone; the wait is for their workers, woken
         * by the loss, to fault their contexts. */
        constexpr uint32_t renew_wait_ms = 2000u;
        D3d11HostDevice *const device11 = apis_->d3d11_device(gpu.index());
        if (device11 == nullptr || !device11->renew(renew_wait_ms)) return false;
        return d3d_[slot].object().initialize(shares_[slot], sessions_[slot].object().imports(), device11,
                                              apis_->d3d12_device(gpu.index()), apis_->surfaces(gpu.index()),
                                              apis_->d3d9_device(gpu.index()));
    }
    default:
        vulkan_[slot].object().initialize(gpu, apis_->memory_source(gpu.index()), sessions_[slot].object().imports());
        return true;
    }
}

void ExecutorRoot::close(ContextSession *session)
{
    AV_SCOPE("ExecutorRoot::close");
    if (!owns(session)) return;
    const uint32_t slot = slot_of(session);
    /* Its holds go before its backend destroys what it imported, and the
     * backend it ran is shut down with the session: the slot's three others
     * never ran this context and are not touched. */
    shares_[slot].end();
    session->release();
    const uint32_t gpu = gpus_of_[slot];
    apis_of_[slot] = 0u;
    gpus_of_[slot] = 0u;
    retire(gpu);
}

uint64_t ExecutorRoot::imported_handle(ContextSession *session, uint64_t blob_id)
{
    AV_SCOPE("ExecutorRoot::imported_handle");
    return owns(session) ? session->imported_handle(blob_id) : 0u;
}

uint32_t ExecutorRoot::share_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                                    uint64_t wire_handle)
{
    AV_SCOPE("ExecutorRoot::share_object");
    const uint32_t slot = slot_of(session);
    if (slot == max_sessions || session->faulted()) return AV_SHARED_REFUSED;
    return shared_answer(shares_[slot].publish(object_id, kind, size, wire_handle));
}

uint32_t ExecutorRoot::open_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                                   uint64_t into, uint64_t &wire_handle)
{
    AV_SCOPE("ExecutorRoot::open_object");
    wire_handle = 0u;
    const uint32_t slot = slot_of(session);
    if (slot == max_sessions || session->faulted()) return AV_SHARED_REFUSED;
    return shared_answer(shares_[slot].open(object_id, kind, size, into, wire_handle));
}

uint32_t ExecutorRoot::shared_answer(Status answered) const
{
    AV_SCOPE("ExecutorRoot::shared_answer");
    switch (answered) {
    case Status::ok: return AV_SHARED_DONE;
    case Status::not_found: return AV_SHARED_UNKNOWN;
    case Status::invalid_parameter: return AV_SHARED_MISMATCHED;
    case Status::conflict: return AV_SHARED_CONFLICT;
    default: return AV_SHARED_REFUSED;
    }
}

/* What a close frees is given back when the guest or a session next lets go
 * on the main loop: the session's own worker may hold the Direct3D 11 lock. */
void ExecutorRoot::close_object(ContextSession *session, uint64_t object_id, uint64_t wire_handle)
{
    AV_SCOPE("ExecutorRoot::close_object");
    const uint32_t slot = slot_of(session);
    if (slot != max_sessions) shares_[slot].close(object_id, wire_handle);
}

void ExecutorRoot::set_signal(ContextSession *session, void (*signal)(void *context), void *context)
{
    AV_SCOPE("ExecutorRoot::set_signal");
    if (owns(session)) session->bind_signal(signal, context);
}

uint32_t ExecutorRoot::lose_device(ContextSession *session)
{
    AV_SCOPE("ExecutorRoot::lose_device");
    return owns(session) ? session->lose_device() : AV_UNKNOWN;
}

void ExecutorRoot::report(StateWriter &writer)
{
    AV_SCOPE("ExecutorRoot::report");
    writer.open("executor", 0u);
    writer.number("sessions", max_sessions);
    writer.number("live", live());
    writer.number("gpus", gpus_.count());
    writer.flag("overflowed", gpus_.overflowed());
    gpus_.report(writer);
    domains_.report(writer);
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (!sessions_[slot].object().live()) continue;
        writer.open("session", slot);
        writer.text("api", api_name(apis_of_[slot]));
        writer.number("gpu", gpus_of_[slot]);
        writer.number("domain", shares_[slot].domain());
        writer.close();
    }
    /* No GPU is counted before the host is asked. */
    for (uint32_t index = 0u; index < gpus_.count(); index++) {
        writer.open("gpu_shares", index);
        D3d11HostDevice *const  device = apis_->d3d11_device(index);
        HostMemorySource *const source = apis_->memory_source(index);
        apis_->shares(index)->report(writer);
        if (device != nullptr) device->report(writer);
        if (source != nullptr) source->report(writer);
        writer.close();
    }
    writer.close();
}

const char *ExecutorRoot::api_name(uint32_t sub_api) const
{
    AV_SCOPE("ExecutorRoot::api_name");
    switch (sub_api) {
    case AV_EXECUTOR_SUB_API_VULKAN: return "vulkan";
    case AV_EXECUTOR_SUB_API_OPENCL: return "opencl";
    case AV_EXECUTOR_SUB_API_OPENGL: return "opengl";
    case AV_EXECUTOR_SUB_API_D3D: return "d3d";
    default: return "unknown";
    }
}

uint32_t ExecutorRoot::live() const
{
    AV_SCOPE("ExecutorRoot::live");
    uint32_t running = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (sessions_[slot].object().live()) running++;
    }
    return running;
}

bool ExecutorRoot::owns(const ContextSession *session) const
{
    AV_SCOPE("ExecutorRoot::owns");
    return slot_of(session) != max_sessions;
}

uint32_t ExecutorRoot::slot_of(const ContextSession *session) const
{
    AV_SCOPE("ExecutorRoot::slot_of");
    for (uint32_t slot = 0u; slot < max_sessions; slot++)
        if (&sessions_[slot].object() == session) return slot;
    return max_sessions;
}

const HostGpu *ExecutorRoot::gpu_of(uint32_t domain) const
{
    AV_SCOPE("ExecutorRoot::gpu_of");
    uint32_t index = 0u;
    return apis_ != nullptr && domains_.gpu_of(domain, index) ? gpus_.at(index) : nullptr;
}

void ExecutorRoot::retire(uint32_t gpu)
{
    AV_SCOPE("ExecutorRoot::retire");
    D3d11HostDevice *const device = apis_->d3d11_device(gpu);
    if (device != nullptr) device->retire_shares();
}

bool ExecutorRoot::invariants_hold() const
{
    AV_SCOPE("ExecutorRoot::invariants_hold");
    return gpus_.invariants_hold() && (apis_ != nullptr || gpus_.count() == 0u);
}

}  // namespace av
