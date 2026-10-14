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
#include "executor/context_session.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/executor_domains.hpp"
#include "executor/gpu_clock_hold.hpp"
#include "executor/host_gpu_set.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/vulkan/vulkan_backend.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SurfacePoint;

class HostApis;
class HostMemorySource;
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
    /* A domain for a guest device on the GPU `gpu_uuid` names: what every
     * other call below and every context of the device names it by. 0 for a
     * GPU nobody has, or no room. Main loop. */
    uint32_t open_domain(const uint8_t *gpu_uuid);
    /* The device is going: every share in `domain` is released, as a reset
     * releases them, its memory is let go of, and the domain names nothing
     * more. Main loop. */
    void     close_domain(uint32_t domain);
    /* Host memory on `domain`'s GPU under `key`, and let go of
     * (HostMemorySource): no_memory for a domain not open or a GPU with no
     * source, and the source's answer otherwise. Main loop. */
    [[nodiscard]] Status allocate_memory(uint32_t domain, uint64_t key, uint64_t size, uint32_t request,
                                         AvHostMemory &memory);
    void                 free_memory(uint32_t domain, uint64_t key);
    /* The image shared as `id` in `domain`, copied into an exportable image. */
    bool     export_image(uint32_t domain, uint64_t id, AvExportedImage &image);
    /* A display head's planes composed into one exported image
     * (D3d11HostDevice::compose_planes). */
    bool     compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                            uint32_t height, AvExportedImage &image);
    /* The guest let go of `id` everywhere, or of every id for 0 - a reset:
     * HostShares::release, and the frames staged of it. Main loop. */
    void     release_shared(uint32_t domain, uint64_t id);
    bool     read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width, uint32_t height);
    /* A present's staged frame of a surface in `domain`, mapped and let go
     * (SharedSurfaces). Any thread. */
    bool     map_frame(uint32_t domain, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view);
    void     unmap_frame(uint32_t domain, const SurfacePoint &frame);
    /* The surface shared as `surface` in `domain` as its own memory, for a
     * process outside QEMU (D3d11HostDevice::export_surface), and the copies
     * into it the host has run with a sync_file for them
     * (D3d11HostDevice::sync_surface). Any thread. */
    bool     export_surface(uint32_t domain, uint64_t surface, AvExportedImage &image);
    bool     sync_surface(uint32_t domain, uint64_t surface, uint64_t &writes, int &fd);

    /* A session for one context of `sub_api` in the domain `options` names, on
     * that domain's GPU, or null: for a sub-API that GPU does not run, for a
     * domain not open, and when every slot is taken. The device turns a null into a context fault
     * and keeps running, so this is a refusal rather than a failure.
     *
     * The backend is the session's own, not shared: a context that dies still
     * owning a VkInstance has to take it with it, and one shared by every
     * session has nowhere to do that (feature 042, found by fuzzing).
     *
     * Only OpenGL takes options other than the defaults, and a `share` (null:
     * none), which must be a live OpenGL session of this root on the same GPU
     * (feature 050); anything else is null. */
    ContextSession *open(uint32_t sub_api, const AvContextOptions &options, ContextSession *share = nullptr);
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
    /* Feature 053 on a session this root handed out, through its domain's
     * HostShares (publish, open, close), answered as AV_SHARED_*: refused,
     * and nothing done, for a faulted session or one it did not hand out -
     * the device's pointer is all the ABI gives to tell them apart. */
    uint32_t share_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                          uint64_t wire_handle);
    uint32_t open_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                         uint64_t &wire_handle);
    void                 close_object(ContextSession *session, uint64_t object_id);
    uint32_t        live() const;
    /* The library's state dump (docs/trace.md): the GPUs asked about, the
     * domains, each live session's API and GPU, and each GPU's shares. Takes
     * no lock of its own: a session's words are written by its own thread,
     * and HostShares reports busy rather than wait. */
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
    [[nodiscard]] bool bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const AvContextOptions &options,
                            const OpenGlBackend *share);
    /* The GPU `domain` is on, or null. */
    const HostGpu *gpu_of(uint32_t domain) const;
    /* The memory source of the GPU `domain` is on, or null. */
    HostMemorySource *source_of(uint32_t domain) const;
    /* The slot `session` is in, or max_sessions for one not ours. */
    uint32_t       slot_of(const ContextSession *session) const;
    /* What the ABI calls `answered`. */
    uint32_t       shared_answer(Status answered) const;
    /* The Direct3D 11 device of the GPU at `gpu` gives back what its shares
     * let go of. */
    void           retire(uint32_t gpu);
    /* The OpenGL backend `share` runs on, when it is a live OpenGL session of
     * this root on `gpu`; null otherwise. */
    const OpenGlBackend *sharing(const ContextSession *share, const HostGpu &gpu) const;
    /* Whether `session` is one of ours rather than a pointer the device
     * invented or already gave back. */
    bool owns(const ContextSession *session) const;
    /* What a dump calls a sub-API. */
    const char *api_name(uint32_t sub_api) const;

    HostGpuSet      gpus_;
    ExecutorDomains domains_;
    GpuClockHold   clocks_[HostGpuSet::max_gpus];
    ContextSession sessions_[max_sessions];
    VulkanBackend  vulkan_[max_sessions];
    OpenClBackend  opencl_[max_sessions];
    OpenGlBackend  opengl_[max_sessions];
    D3dBackend     d3d_[max_sessions];
    /* What each session shares through, bound as it opens. */
    SessionShares  shares_[max_sessions];
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
