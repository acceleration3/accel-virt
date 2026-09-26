/* The executor library the glue loaded, or none: its operations table once
 * checked, and the calls into it. The table outlives the device. */
#ifndef AV_EXECUTOR_LIBRARY_HPP
#define AV_EXECUTOR_LIBRARY_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary {
public:
    ExecutorLibrary();

    /* Null leaves the device without an executor. invalid_parameter for a
     * table of another ABI version or missing an operation its version has;
     * invalid_state once a table is accepted. */
    [[nodiscard]] Status accept(const RvkExecutorOps *ops);
    bool                 available() const;
    /* The sub-APIs the library offers that the protocol knows; 0 without one. */
    uint32_t             sub_apis() const;
    /* Null without a library: a context's worker asks whether or not one was
     * loaded, and the device core's own tests bind contexts with none.
     * `options` null is the defaults and
     * `share` null shares nothing; a library before version 4 makes only
     * that, and anything else is null without a call. */
    RvkExecutor         *create(SubApi api, const uint8_t *gpu_uuid, const RvkContextOptions *options = nullptr,
                                RvkExecutor *share = nullptr) const;
    /* Whether the library makes a context with options (version 4). */
    bool                 names_options() const;
    /* Whether the library can be given something to call when a host API
     * finishes work on a thread of its own: version 5 onwards. A version 4
     * library signals nothing, and a guest on one waits for its events at a
     * synchronising call instead. */
    bool                 names_events() const;
    /* Gives `executor` that something, or takes it back with a null. Nothing
     * for a library that names no events. */
    void                 set_signal(RvkExecutor *executor, void (*signal)(void *context),
                                    void *context) const;
    void                 destroy(RvkExecutor *executor) const;
    void                 end_batch(RvkExecutor *executor) const;
    bool                 poll(RvkExecutor *executor) const;
    /* Version 11's gpu_progress; both 0 from an older library. */
    void                 gpu_progress(RvkExecutor *executor, uint64_t &marked, uint64_t &reached) const;
    uint32_t             execute(RvkExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &reply_size) const;
    /* Whether this library carries version 2's memory channel at all. A
     * version 1 library is a library; it just imports nothing. */
    bool                 imports() const;
    /* Which RVK_BLOB_* kinds `api` imports on the GPU `gpu_uuid` names, or 0.
     * Asked on the main loop while the capset is being described. */
    uint32_t             import_kinds(SubApi api, const uint8_t *gpu_uuid) const;
    /* The host image under `key`, read into `pixels` (version 7); false for a
     * library too old to or a key it does not know. */
    bool                 export_image(const uint8_t *gpu_uuid, uint64_t key, RvkExportedImage &image) const;
    bool                 read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride,
                                    uint32_t width, uint32_t height) const;
    /* Whether the library answers the wire handle an import made (version 3),
     * without which a guest has nothing to name an import by. */
    bool                 names_imports() const;
    /* Only on a library that names imports. */
    uint32_t             import_blob(RvkExecutor *executor, uint64_t blob_id, const RvkBlobHandle &handle) const;
    void                 release_blob(RvkExecutor *executor, uint64_t blob_id) const;
    uint64_t             imported_handle(RvkExecutor *executor, uint64_t blob_id) const;

    /* Whether the library moves one host object between two executors
     * (version 6), without which a shared object has no host resource behind
     * it and every shared-object command is refused rather than faulted. */
    bool                 shares_objects() const;
    /* Only on a library that shares objects. A descriptor for what
     * `wire_handle` names, which stays the caller's afterwards; false for a
     * handle this executor did not make or a kind it cannot export. What a
     * library that answers true with nothing behind it said is judged by
     * HostSharedObject, which is where the descriptor is taken. */
    [[nodiscard]] bool   export_object(RvkExecutor *executor, uint32_t kind, uint64_t wire_handle,
                                       uint32_t &handle_kind, int &fd) const;
    /* That descriptor made into an object of `executor`'s, with the handle the
     * guest names it by; false when the library will not. `fd` stays the
     * caller's whatever the answer. */
    [[nodiscard]] bool   import_object(RvkExecutor *executor, uint32_t kind, uint32_t handle_kind, int fd,
                                       uint64_t size, uint64_t &wire_handle) const;
    void                 release_object(RvkExecutor *executor, uint32_t kind, uint64_t wire_handle) const;

    /* Every sub-API this device knows: what a capset offers before the library
     * that answers the real mask has been asked. */
    static constexpr uint32_t known_sub_apis = 15u;

private:
    /* RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF: a kind this device does not
     * know is a kind it cannot build a handle for, so it is not offered. */
    static constexpr uint32_t known_handle_kinds = 3u;

    const RvkExecutorOps *ops_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LIBRARY_HPP
