#include "device/executor/executor_root.hpp"

#include "device/shim/d3d11_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/host_apis.hpp"
#include "device/executor/host_gpu.hpp"

namespace av {

namespace {

/* One decode block per session, in the library's own storage rather than in
 * the root: sixty four megabytes is not something to carry inside the object
 * that owns the sessions, and nothing but this file ever names it. */
alignas(16) uint8_t decode_blocks[ExecutorRoot::sessions][DecodeArena::arena_bytes];

}  // namespace

ExecutorRoot::ExecutorRoot()
    : gpus_(), sessions_{}, vulkan_{}, opencl_{}, opengl_{}, d3d_{}, apis_of_{}, gpus_of_{}, apis_(nullptr)
{
    AV_SCOPE("ExecutorRoot::ExecutorRoot");
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

uint32_t ExecutorRoot::import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const
{
    AV_SCOPE("ExecutorRoot::import_kinds");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    return gpu != nullptr ? gpu->import_kinds(sub_api) : 0u;
}

bool ExecutorRoot::export_image(const uint8_t *gpu_uuid, uint64_t key, RvkExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::export_image");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->export_image(key, image);
}

void ExecutorRoot::release_shared(const uint8_t *gpu_uuid, uint64_t object_id)
{
    AV_SCOPE("ExecutorRoot::release_shared");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    if (device != nullptr) device->release_shared(object_id);
}

bool ExecutorRoot::read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                              uint32_t height)
{
    AV_SCOPE("ExecutorRoot::read_image");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->read_image(key, pixels, stride, width, height);
}

ContextSession *ExecutorRoot::open(uint32_t sub_api, const uint8_t *gpu_uuid, const RvkContextOptions *options,
                                   ContextSession *share)
{
    AV_SCOPE("ExecutorRoot::open");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    if (gpu == nullptr) return nullptr;
    const bool asked = share != nullptr || (options != nullptr && (options->major != 0u || options->minor != 0u ||
                                                                   options->profile != 0u || options->flags != 0u ||
                                                                   options->reserved != 0u));
    if (asked && sub_api != RVK_EXECUTOR_SUB_API_OPENGL) return nullptr;
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
        if (sessions_[slot].initialize(*backend_in(slot, sub_api), decode_blocks[slot],
                                       DecodeArena::arena_bytes) != Status::ok)
            continue;
        /* Opened here, at the context's setup, on what the host found when it
         * was asked: the guest's first batch waits behind this, inside
         * dxgkrnl's timeout, so it may cost a context and never a driver
         * start-up (feature 044, T035). A GPU the capset said runs the API can
         * still refuse a context, and then the slot goes back. */
        if (!bind(slot, sub_api, *gpu, options, shared)) {
            sessions_[slot].release();
            return nullptr;
        }
        apis_of_[slot] = sub_api;
        gpus_of_[slot] = gpu->index();
        /* Not OpenCL's: its kernels saturate the GPU between the guest's
         * waits, and the peak state's fixed clock is below what a saturated
         * GPU boosts to - held, the OpenCL samples lost 5 to 10%. */
        if (sub_api != RVK_EXECUTOR_SUB_API_OPENCL && clocks_[gpu->index()].open(apis_->render_minor(gpu->index())))
            sessions_[slot].hold_clocks(clocks_[gpu->index()]);
        return &sessions_[slot];
    }
    return nullptr;
}

SessionBackend *ExecutorRoot::backend_in(uint32_t slot, uint32_t sub_api)
{
    AV_SCOPE("ExecutorRoot::backend_in");
    switch (sub_api) {
    case RVK_EXECUTOR_SUB_API_VULKAN: return &vulkan_[slot];
    case RVK_EXECUTOR_SUB_API_OPENCL: return &opencl_[slot];
    case RVK_EXECUTOR_SUB_API_OPENGL: return &opengl_[slot];
    case RVK_EXECUTOR_SUB_API_D3D: return &d3d_[slot];
    default: return nullptr;
    }
}

const OpenGlBackend *ExecutorRoot::sharing(const ContextSession *share, const HostGpu &gpu) const
{
    AV_SCOPE("ExecutorRoot::sharing");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot] != share) continue;
        const bool fits = share->live() && apis_of_[slot] == RVK_EXECUTOR_SUB_API_OPENGL &&
                          gpus_of_[slot] == gpu.index();
        return fits ? &opengl_[slot] : nullptr;
    }
    return nullptr;
}

bool ExecutorRoot::bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const RvkContextOptions *options,
                        const OpenGlBackend *share)
{
    AV_SCOPE("ExecutorRoot::bind");
    switch (sub_api) {
    /* The GPU's EGL display goes with its OpenCL device: on a host whose
     * OpenCL cannot take a descriptor, OpenGL takes it and OpenCL names the
     * GL object (feature 053, T019). A host with no display shares nothing
     * and runs everything else. */
    case RVK_EXECUTOR_SUB_API_OPENCL:
        return opencl_[slot].initialize(apis_->opencl_device(gpu.index()), apis_->gl_display(gpu.index()));
    case RVK_EXECUTOR_SUB_API_OPENGL:
        return opengl_[slot].initialize(apis_->gl_display(gpu.index()), options, share);
    case RVK_EXECUTOR_SUB_API_D3D:
        return d3d_[slot].initialize(apis_->d3d11_device(gpu.index()), apis_->d3d12_device(gpu.index()));
    default: vulkan_[slot].initialize(gpu); return true;
    }
}

void ExecutorRoot::close(ContextSession *session)
{
    AV_SCOPE("ExecutorRoot::close");
    if (!owns(session)) return;
    session->release();
    /* And the host objects the guest never destroyed. A context that dies
     * owning a VkInstance would otherwise leak it for the life of the process,
     * which is a guest's to trigger as often as it likes. */
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot] != session) continue;
        apis_of_[slot] = 0u;
        gpus_of_[slot] = 0u;
        vulkan_[slot].shut_down();
        opencl_[slot].shut_down();
        opengl_[slot].shut_down();
        d3d_[slot].shut_down();
    }
}

uint64_t ExecutorRoot::imported_handle(ContextSession *session, uint64_t blob_id)
{
    AV_SCOPE("ExecutorRoot::imported_handle");
    return owns(session) ? session->imported_handle(blob_id) : 0u;
}

bool ExecutorRoot::export_object(ContextSession *session, uint32_t kind, uint64_t wire_handle,
                                 uint32_t &handle_kind, int &fd)
{
    AV_SCOPE("ExecutorRoot::export_object");
    handle_kind = 0u;
    fd = -1;
    return owns(session) && session->export_object(kind, wire_handle, handle_kind, fd);
}

bool ExecutorRoot::import_object(ContextSession *session, uint32_t kind, uint32_t handle_kind, int fd,
                                 uint64_t size, uint64_t &wire_handle)
{
    AV_SCOPE("ExecutorRoot::import_object");
    wire_handle = 0u;
    return owns(session) && session->import_object(kind, handle_kind, fd, size, wire_handle);
}

void ExecutorRoot::release_object(ContextSession *session, uint32_t kind, uint64_t wire_handle)
{
    AV_SCOPE("ExecutorRoot::release_object");
    if (owns(session)) session->release_object(kind, wire_handle);
}

void ExecutorRoot::set_signal(ContextSession *session, void (*signal)(void *context), void *context)
{
    AV_SCOPE("ExecutorRoot::set_signal");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot] != session) continue;
        if (apis_of_[slot] == RVK_EXECUTOR_SUB_API_OPENCL) opencl_[slot].bind_signal(signal, context);
        if (apis_of_[slot] == RVK_EXECUTOR_SUB_API_D3D) d3d_[slot].bind_signal(signal, context);
        return;
    }
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
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (!sessions_[slot].live()) continue;
        writer.open("session", slot);
        writer.text("api", api_name(apis_of_[slot]));
        writer.number("gpu", gpus_of_[slot]);
        writer.close();
    }
    writer.close();
}

const char *ExecutorRoot::api_name(uint32_t sub_api) const
{
    AV_SCOPE("ExecutorRoot::api_name");
    switch (sub_api) {
    case RVK_EXECUTOR_SUB_API_VULKAN: return "vulkan";
    case RVK_EXECUTOR_SUB_API_OPENCL: return "opencl";
    case RVK_EXECUTOR_SUB_API_OPENGL: return "opengl";
    case RVK_EXECUTOR_SUB_API_D3D: return "d3d";
    default: return "unknown";
    }
}

uint32_t ExecutorRoot::live() const
{
    AV_SCOPE("ExecutorRoot::live");
    uint32_t running = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (sessions_[slot].live()) running++;
    }
    return running;
}

bool ExecutorRoot::owns(const ContextSession *session) const
{
    AV_SCOPE("ExecutorRoot::owns");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot] == session) return true;
    }
    return false;
}

bool ExecutorRoot::invariants_hold() const
{
    AV_SCOPE("ExecutorRoot::invariants_hold");
    return gpus_.invariants_hold() && (apis_ != nullptr || gpus_.count() == 0u);
}

}  // namespace av
