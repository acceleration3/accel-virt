/* What the executor library is, behind the one symbol it exports.
 *
 * Named a root rather than a library because the device already has an
 * `av::ExecutorLibrary` - the table it loaded - and these are opposite ends of
 * the same ABI. This is the end that answers.
 *
 * There is exactly one of these in a process, in `ObjectStorage` in the
 * translation unit that defines the C entry points, because `rvk_executor_ops()`
 * takes no argument and so has nowhere to put a `this` (CLAUDE.md). */
#ifndef AV_EXECUTOR_ROOT_HPP
#define AV_EXECUTOR_ROOT_HPP

#include "core/support/status.hpp"
#include "device/executor/context_session.hpp"
#include "device/executor/host_gpu_set.hpp"
#include "device/executor/opencl_backend.hpp"
#include "device/executor/opengl_backend.hpp"
#include "device/executor/vulkan_backend.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostApis;
class HostGpu;
class SessionBackend;

class ExecutorRoot {
public:
    /* Sessions this library will hand out at once. */
    static constexpr uint32_t sessions = 64u;

    ExecutorRoot();
    ExecutorRoot(const ExecutorRoot &) = delete;
    ExecutorRoot &operator=(const ExecutorRoot &) = delete;

    /* Asks the host's loaders once, on QEMU's main loop, before any context:
     * the answer to sub_apis() is frozen into the capset for the device's
     * lifetime, so it has to be answerable with no GPU context in hand. `apis`
     * must outlive this root: contexts open on what it found. */
    [[nodiscard]] Status initialize(HostApis &apis);

    /* Every sub-API some GPU here can run. Zero is an answer: the device then
     * describes no capset at all rather than one it cannot honour. */
    uint32_t sub_apis() const;
    /* The RVK_BLOB_* kinds `sub_api` imports on the GPU `gpu_uuid` names.
     * Zero for a GPU nobody has, which is also what a null UUID gets. */
    uint32_t import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const;

    /* A session for one context of `sub_api` on the GPU `gpu_uuid` names, or
     * null: for a sub-API that GPU does not run, for a GPU nobody has, and
     * when every slot is taken. The device turns a null into a context fault
     * and keeps running, so this is a refusal rather than a failure.
     *
     * The backend is the session's own, not shared: a context that dies still
     * owning a VkInstance has to take it with it, and one shared by every
     * session has nowhere to do that (feature 042, found by fuzzing). */
    ContextSession *open(uint32_t sub_api, const uint8_t *gpu_uuid);
    /* The backend a session is running on, for a test to look at. */
    SessionBackend *backend_of(const ContextSession *session);
    /* Gives a session's slot and block back. Safe on a null, which is what the
     * device passes if it ever destroys twice. */
    void            close(ContextSession *session);
    /* What `session` answers for `blob_id`, or 0 for a session this root
     * never handed out - a null included - because the device's pointer is
     * the only thing the ABI gives to tell them apart. */
    uint64_t        imported_handle(ContextSession *session, uint64_t blob_id);
    uint32_t        live() const;

    bool invariants_hold() const;

private:
    friend struct ExecutorRootProbe;

    /* DeviceContextTable::max_contexts, which the capset publishes: the device
     * can never ask for another, so a second ceiling would be one nobody
     * enforces. */
    static constexpr uint32_t max_sessions = sessions;

    /* The backend of `sub_api` in `slot`, or null for a value that is not one
     * of the three sub-APIs. */
    SessionBackend *backend_in(uint32_t slot, uint32_t sub_api);
    /* Binds the backend of `sub_api` in `slot` to `gpu`, with what the host
     * found for it: false when the host found nothing for this API there. */
    [[nodiscard]] bool bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu);
    /* Whether `session` is one of ours rather than a pointer the device
     * invented or already gave back. */
    bool owns(const ContextSession *session) const;

    HostGpuSet     gpus_;
    ContextSession sessions_[max_sessions];
    VulkanBackend  vulkan_[max_sessions];
    OpenClBackend  opencl_[max_sessions];
    OpenGlBackend  opengl_[max_sessions];
    /* The host that was asked, which keeps what a context runs on - its
     * display, its OpenCL device - for as long as this root. Null until then. */
    HostApis      *apis_;
};

}  // namespace av

#endif  // AV_EXECUTOR_ROOT_HPP
