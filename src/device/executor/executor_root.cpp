#include "device/executor/executor_root.hpp"

#include "core/trace/function_scope.hpp"
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

ExecutorRoot::ExecutorRoot() : gpus_(), sessions_{}, vulkan_{}, opencl_{}, opengl_{}, apis_(nullptr)
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

ContextSession *ExecutorRoot::open(uint32_t sub_api, const uint8_t *gpu_uuid)
{
    AV_SCOPE("ExecutorRoot::open");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    if (gpu == nullptr) return nullptr;
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
        if (!bind(slot, sub_api, *gpu)) {
            sessions_[slot].release();
            return nullptr;
        }
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
    default: return nullptr;
    }
}

bool ExecutorRoot::bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu)
{
    AV_SCOPE("ExecutorRoot::bind");
    switch (sub_api) {
    case RVK_EXECUTOR_SUB_API_OPENCL: return opencl_[slot].initialize(apis_->opencl_device(gpu.index()));
    case RVK_EXECUTOR_SUB_API_OPENGL: return opengl_[slot].initialize(apis_->gl_display(gpu.index()));
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
        vulkan_[slot].shut_down();
        opencl_[slot].shut_down();
        opengl_[slot].shut_down();
    }
}

uint64_t ExecutorRoot::imported_handle(ContextSession *session, uint64_t blob_id)
{
    AV_SCOPE("ExecutorRoot::imported_handle");
    return owns(session) ? session->imported_handle(blob_id) : 0u;
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
