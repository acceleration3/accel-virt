/* Where a device's contexts run: in QEMU's own process, or in a renderer
 * process that a crash in the host GPU stack takes down alone
 * (docs/renderer.md). The device's control plane - its commands, blobs,
 * heads and interrupts - asks this for everything that touches an executor.
 * Main loop only. */
#ifndef AV_CONTEXT_PLANE_HPP
#define AV_CONTEXT_PLANE_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {


class SurfacePoint;

class CapsetInfo;
class ContextSetup;
class StateWriter;

class ContextPlane {
public:
    /* The executor library's table: invalid_parameter for one this ABI does
     * not accept. */
    [[nodiscard]] virtual Status accept(const AvExecutorOps *executor) = 0;
    /* The locks the contexts share: no_memory when the host has none. */
    [[nodiscard]] virtual Status initialize() = 0;
    /* Where the contexts run is made ready for GPU `gpu_uuid` and a capset
     * offering at least what the one the guest reads will: the renderer is
     * started and greeted, and the library opens the device's domain there.
     * Whatever went wrong, named for the log. */
    [[nodiscard]] virtual Status start(const uint8_t *gpu_uuid, const CapsetInfo &capset) = 0;
    virtual uint32_t             sub_apis() const = 0;
    /* Main loop. `size` bytes of the host memory `request` names, for
     * context `context`'s process, kept under `key` until free_memory
     * (docs/host-memory.md): its dma-buf, the caller's, in `memory`.
     * no_memory when there is none to give. */
    [[nodiscard]] virtual Status allocate_memory(uint32_t context, uint64_t key, uint64_t size, uint32_t request,
                                                 AvHostMemory &memory) = 0;
    /* Main loop. The memory under `key` let go; an import still holding it
     * keeps it until the import goes. */
    virtual void                 free_memory(uint64_t key) = 0;

    /* invalid_parameter for id 0, conflict for a live id, no_space when full. */
    [[nodiscard]] virtual Status create(uint32_t id) = 0;
    [[nodiscard]] virtual Status destroy(uint32_t id) = 0;
    /* Every context. */
    virtual void                 destroy_all() = 0;
    virtual bool                 holds(uint32_t id) = 0;
    /* not_found for a resource the setup names that is not there, no_memory,
     * or invalid_parameter for anything else refused. */
    [[nodiscard]] virtual Status bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset) = 0;
    virtual bool                 holds_doorbell(uint32_t index) = 0;
    virtual bool                 can_share(uint32_t asking, uint32_t share) = 0;
    virtual uint32_t             live() const = 0;

    /* The device's blob table changed, or its fence interrupts: contexts in
     * another process are told, since they hold a copy of what they read. */
    virtual void blob_created(uint32_t id) = 0;
    virtual void blob_released(uint32_t id) = 0;
    /* Main loop. The guest let go of the shared object `object_id` everywhere
     * (SharedReleaseRequest): what was shared under it on the host GPU
     * `gpu_uuid` goes once nothing on the host still holds it. Object 0 is
     * every one, which the device sends at a reset. */
    virtual void shared_released(const uint8_t *gpu_uuid, uint64_t object_id) = 0;
    virtual void interrupts_changed(bool connected) = 0;
    /* The process the contexts ran in died: they are lost, and it is
     * started again. */
    virtual void renderer_lost() = 0;

    /* After every Direct3D stream has run what it published. */
    virtual bool export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image) = 0;
    /* Whether a head's planes can be composed here, and `count` of them
     * composed into one `width` by `height` image handed over as export_image
     * hands one over (AvExecutorOps::compose_planes). Any thread: an export
     * connection asks too. */
    virtual bool composes() const = 0;
    virtual bool compose_planes(const uint8_t *gpu_uuid, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                uint32_t height, AvExportedImage &image) = 0;
    virtual bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                            uint32_t height) = 0;
    /* Any thread: whether a present snapshot a Direct3D session took can be
     * read here, and the one under `key` mapped and let go
     * (ExecutorLibrary::map_frame). */
    virtual bool reads_frames() const = 0;
    virtual bool map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) = 0;
    virtual void unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame) = 0;
    /* Any thread: whether a surface the guest shared can be handed to a
     * process outside QEMU here (docs/external-export.md), and the one
     * shared as `surface` exported as its own memory, or the copies into it
     * counted with a sync_file for them (ExecutorLibrary::export_surface,
     * sync_surface). Every descriptor answered is the caller's. */
    virtual bool exports_surfaces() const = 0;
    virtual bool export_surface(const uint8_t *gpu_uuid, uint64_t surface, AvExportedImage &image) = 0;
    virtual bool sync_surface(const uint8_t *gpu_uuid, uint64_t surface, uint64_t &writes, int &fd) = 0;

    virtual void report(StateWriter &writer) = 0;
    virtual bool invariants_hold() const = 0;

protected:
    ContextPlane() = default;
    ~ContextPlane() = default;
    ContextPlane(const ContextPlane &) = delete;
    ContextPlane &operator=(const ContextPlane &) = delete;
};

}  // namespace av

#endif  // AV_CONTEXT_PLANE_HPP
