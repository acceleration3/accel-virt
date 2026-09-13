#include "device/executor/executor_root.hpp"

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

ExecutorRoot::ExecutorRoot() : gpus_(), sessions_{}, vulkan_{}, opencl_{}, opengl_{}, asked_(false) {}

Status ExecutorRoot::initialize(HostApis &apis)
{
    if (asked_) return Status::invalid_state;
    const Status status = gpus_.initialize(apis);
    /* A host with more GPUs than the set holds is a host this still serves,
     * with the ones it kept. Anything else is a failure to answer at all. */
    if (status != Status::ok && status != Status::no_space) return status;
    asked_ = true;
    return Status::ok;
}

uint32_t ExecutorRoot::sub_apis() const { return asked_ ? gpus_.sub_apis() : 0u; }

uint32_t ExecutorRoot::import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const
{
    const HostGpu *const gpu = asked_ ? gpus_.find(gpu_uuid) : nullptr;
    return gpu != nullptr ? gpu->import_kinds(sub_api) : 0u;
}

ContextSession *ExecutorRoot::open(uint32_t sub_api, const uint8_t *gpu_uuid)
{
    const HostGpu *const gpu = asked_ ? gpus_.find(gpu_uuid) : nullptr;
    if (gpu == nullptr) return nullptr;
    /* A sub-API this GPU does not run is a context the guest should never have
     * been allowed to ask for, but the capset is the union across GPUs, so it
     * could have been. */
    if ((gpu->sub_apis() & sub_api) == 0u) return nullptr;

    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (sessions_[slot].live()) continue;
        SessionBackend *backend = nullptr;
        if (sub_api == RVK_EXECUTOR_SUB_API_VULKAN) backend = &vulkan_[slot];
        if (sub_api == RVK_EXECUTOR_SUB_API_OPENCL) backend = &opencl_[slot];
        if (sub_api == RVK_EXECUTOR_SUB_API_OPENGL) {
            /* The only backend that has to open something before it can run
             * anything: a GL context belongs to a GPU and is made for one. */
            if (!opengl_[slot].initialize(gpu_uuid)) return nullptr;
            backend = &opengl_[slot];
        }
        /* A sub-API with no backend yet is refused rather than given a session
         * that would answer nothing: OpenGL is the one, and it is refused at
         * the capset too, because no GPU here reports running it. */
        if (backend == nullptr) return nullptr;
        if (sessions_[slot].initialize(*backend, decode_blocks[slot], DecodeArena::arena_bytes) !=
            Status::ok)
            return nullptr;
        return &sessions_[slot];
    }
    return nullptr;
}

void ExecutorRoot::close(ContextSession *session)
{
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

uint32_t ExecutorRoot::live() const
{
    uint32_t running = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (sessions_[slot].live()) running++;
    }
    return running;
}

bool ExecutorRoot::owns(const ContextSession *session) const
{
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot] == session) return true;
    }
    return false;
}

bool ExecutorRoot::invariants_hold() const
{
    return gpus_.invariants_hold() && (asked_ || gpus_.count() == 0u);
}

}  // namespace av
