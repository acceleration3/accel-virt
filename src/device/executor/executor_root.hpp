/* What the executor library is, behind the one symbol it exports.
 *
 * Named a root rather than a library because the device already has an
 * `av::ExecutorLibrary` - the table it loaded - and these are opposite ends of
 * the same ABI. This is the end that answers.
 *
 * There is exactly one of these in a process, in `ObjectStorage` in the
 * translation unit that defines the C entry points, because `av_executor_ops()`
 * takes no argument and so has nowhere to put a `this` (CLAUDE.md). */
#ifndef AV_EXECUTOR_ROOT_HPP
#define AV_EXECUTOR_ROOT_HPP

#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "device/abi/av_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/d3d_backend.hpp"
#include "device/executor/gpu_clock_hold.hpp"
#include "device/executor/host_gpu_set.hpp"
#include "device/executor/opencl_backend.hpp"
#include "device/executor/opengl_backend.hpp"
#include "device/executor/vulkan_backend.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SurfacePoint;

class HostApis;
class HostGpu;
class SessionBackend;

class ExecutorRoot final : public StateReporter {
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
    /* The AV_BLOB_* kinds `sub_api` imports on the GPU `gpu_uuid` names.
     * Zero for a GPU nobody has, which is also what a null UUID gets. */
    uint32_t import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const;
    /* The host image under `key` on the GPU `gpu_uuid` names, into `pixels`. */
    bool     export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image);
    /* The Direct3D surface shared under `object_id` on `gpu_uuid` goes: the
     * guest let go of it everywhere. Main loop. */
    void     release_shared(const uint8_t *gpu_uuid, uint64_t object_id);
    bool     read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                        uint32_t height);
    /* The present snapshot under `key` on `gpu_uuid`, mapped and let go
     * (D3d11HostDevice::map_frame). Any thread. */
    bool     map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view);
    void     unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame);

    /* A session for one context of `sub_api` on the GPU `gpu_uuid` names, or
     * null: for a sub-API that GPU does not run, for a GPU nobody has, and
     * when every slot is taken. The device turns a null into a context fault
     * and keeps running, so this is a refusal rather than a failure.
     *
     * The backend is the session's own, not shared: a context that dies still
     * owning a VkInstance has to take it with it, and one shared by every
     * session has nowhere to do that (feature 042, found by fuzzing).
     *
     * `options` (null: the defaults) and `share` (null: none) are version 4's
     * (feature 050): only OpenGL takes options other than the defaults or a
     * share, which must be a live OpenGL session of this root on the same GPU;
     * anything else is null. */
    ContextSession *open(uint32_t sub_api, const uint8_t *gpu_uuid, const AvContextOptions *options = nullptr,
                         ContextSession *share = nullptr);
    /* Gives a session's slot and block back. Safe on a null, which is what the
     * device passes if it ever destroys twice. */
    void            close(ContextSession *session);
    /* What `session` answers for `blob_id`, or 0 for a session this root
     * never handed out - a null included - because the device's pointer is
     * the only thing the ABI gives to tell them apart. */
    uint64_t        imported_handle(ContextSession *session, uint64_t blob_id);
    /* What the device gives an OpenCL context to call when the host finishes
     * work on a thread of its own (executor ABI version 5). Nothing for a
     * session of another API: neither Vulkan nor OpenGL completes anything
     * except on the thread that asked. */
    void            set_signal(ContextSession *session, void (*signal)(void *context), void *context);
    /* The version 6 calls, on a session this root handed out: false, and
     * nothing made, for one it did not - the device's pointer is all the ABI
     * gives to tell them apart. */
    [[nodiscard]] bool export_object(ContextSession *session, uint32_t kind, uint64_t wire_handle,
                                     uint32_t &handle_kind, int &fd);
    [[nodiscard]] bool import_object(ContextSession *session, uint32_t kind, uint32_t handle_kind, int fd,
                                     uint64_t size, uint64_t &wire_handle);
    void               release_object(ContextSession *session, uint32_t kind, uint64_t wire_handle);
    uint32_t        live() const;
    /* The library's state dump (docs/trace.md): the GPUs asked about and each
     * live session's API and GPU. Takes no lock: everything it reads is a
     * plain word a session's own thread writes. */
    void            report(StateWriter &writer) override;

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
    [[nodiscard]] bool bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const AvContextOptions *options,
                            const OpenGlBackend *share);
    /* The OpenGL backend `share` runs on, when it is a live OpenGL session of
     * this root on `gpu`; null otherwise. */
    const OpenGlBackend *sharing(const ContextSession *share, const HostGpu &gpu) const;
    /* Whether `session` is one of ours rather than a pointer the device
     * invented or already gave back. */
    bool owns(const ContextSession *session) const;
    /* What a dump calls a sub-API. */
    const char *api_name(uint32_t sub_api) const;

    HostGpuSet     gpus_;
    GpuClockHold   clocks_[HostGpuSet::max_gpus];
    ContextSession sessions_[max_sessions];
    VulkanBackend  vulkan_[max_sessions];
    OpenClBackend  opencl_[max_sessions];
    OpenGlBackend  opengl_[max_sessions];
    D3dBackend     d3d_[max_sessions];
    /* What each open session runs: its sub-API and its GPU's index, for a
     * share's check and the dump. */
    uint32_t       apis_of_[max_sessions];
    uint32_t       gpus_of_[max_sessions];
    /* The host that was asked, which keeps what a context runs on - its
     * display, its OpenCL device - for as long as this root. Null until then. */
    HostApis      *apis_;
};

}  // namespace av

#endif  // AV_EXECUTOR_ROOT_HPP
