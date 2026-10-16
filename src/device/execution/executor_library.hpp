/* The executor library the glue loaded, or none: its operations table once
 * checked, the domain this device is to it (av_executor.h, open_domain), and
 * the calls into it. The table outlives the device. */
#ifndef AV_EXECUTOR_LIBRARY_HPP
#define AV_EXECUTOR_LIBRARY_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/host_poll.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {


class SurfacePoint;

class ExecutorLibrary {
public:
    ExecutorLibrary();

    /* Null leaves the device without an executor. invalid_parameter for a
     * table of another ABI version or missing any operation; invalid_state
     * once a table is accepted. */
    [[nodiscard]] Status accept(const AvExecutorOps *ops);
    bool                 available() const;
    /* The sub-APIs the library offers that the protocol knows; 0 without one. */
    uint32_t             sub_apis() const;

    /* Main loop, as the device realizes: this device's domain, on the GPU
     * `gpu_uuid` names. ok without a library, which shares nothing;
     * not_found for a GPU the library has not got; invalid_state with one
     * open. */
    [[nodiscard]] Status open_domain(const uint8_t *gpu_uuid);
    /* Main loop: the domain closed, and every object shared in it with it. */
    void                 close_domain();
    uint32_t             domain() const;
    /* Main loop. Host memory in this device's domain, under `key`
     * (AvExecutorOps::allocate_memory): no_memory without a library or a
     * domain, or when it allocated nothing. */
    [[nodiscard]] Status allocate_memory(uint64_t key, uint64_t size, uint32_t request, AvHostMemory &memory) const;
    void                 free_memory(uint64_t key) const;

    /* An executor in this device's domain, made with `options` and sharing
     * `share`'s objects (null: none); null without a library or a domain,
     * or when the library makes none. */
    AvExecutor *create(SubApi api, const AvContextOptions &options, AvExecutor *share) const;
    void        destroy(AvExecutor *executor) const;
    uint32_t    execute(AvExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                        uint8_t *reply, size_t reply_capacity, size_t &reply_size) const;
    void        end_batch(AvExecutor *executor) const;
    /* What AvExecutorOps::poll answered; an answer outside the ABI is
     * idle, which holds nothing up. */
    HostPoll    poll(AvExecutor *executor) const;
    void        gpu_progress(AvExecutor *executor, uint64_t &marked, uint64_t &reached) const;
    /* Gives `executor` something to call when a host API finishes work on a
     * thread of its own, or takes it back with a null. */
    void        set_signal(AvExecutor *executor, void (*signal)(void *context), void *context) const;
    uint32_t    import_blob(AvExecutor *executor, uint64_t blob_id, uint64_t memory_key) const;
    void        release_blob(AvExecutor *executor, uint64_t blob_id) const;
    uint64_t    imported_handle(AvExecutor *executor, uint64_t blob_id) const;
    /* For tests (AvExecutorOps::lose_device): the host device `executor`
     * runs on made lost. not_supported for a library without the call, or a
     * host that cannot. */
    [[nodiscard]] Status lose_device(AvExecutor *executor) const;
    /* What `executor` kept of an answer too long for its reply, from
     * `offset` into `out`: the bytes copied, and the answer's `length`. */
    size_t      answer_part(AvExecutor *executor, uint64_t offset, uint8_t *out, size_t capacity,
                            uint64_t &length) const;

    /* Feature 053, answered as the library answered it (AV_SHARED_*): the
     * object `wire_handle` names in `executor`'s commands shared as
     * `object_id`, and that object opened as one of `executor`'s, made into
     * `into`, with the handle the guest names it by. A library that answers
     * done and names nothing is refused: there would be nothing to give back
     * by. */
    uint32_t    share_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                             uint64_t wire_handle) const;
    uint32_t    open_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                            uint64_t &wire_handle) const;
    void        close_object(AvExecutor *executor, uint64_t object_id, uint64_t wire_handle) const;
    /* The guest let go of `object_id` everywhere, or of every id for 0;
     * nothing without a domain. */
    void        release_shared(uint64_t object_id) const;

    /* The image shared as `key`, read into `pixels` or exported; false
     * without a domain or for a key the library does not know. */
    bool export_image(uint64_t key, AvExportedImage &image) const;
    /* Whether the executor composes a head's planes, and its planes composed
     * (AvExecutorOps::compose_planes). */
    bool composes() const;
    bool compose_planes(const AvScanoutPlane *planes, uint32_t count, uint32_t width, uint32_t height,
                        AvExportedImage &image) const;
    bool read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height) const;
    /* A present's staged frame, mapped and let go. Any thread. */
    bool map_frame(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) const;
    void unmap_frame(const SurfacePoint &frame) const;
    /* A shared surface handed to a process outside QEMU, and the copies into
     * it with a sync_file for them: false, with nothing handed over, without
     * a domain or for a surface the library does not know. Any thread. */
    bool export_surface(uint64_t surface, AvExportedImage &image) const;
    bool sync_surface(uint64_t surface, uint64_t &writes, int &fd) const;

    /* Every sub-API this device knows: what a capset offers before the library
     * that answers the real mask has been asked. */
    static constexpr uint32_t known_sub_apis = 15u;

private:
    /* Whether every operation of the version is there. */
    bool complete(const AvExecutorOps &ops) const;

    const AvExecutorOps *ops_;
    uint32_t             domain_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LIBRARY_HPP
