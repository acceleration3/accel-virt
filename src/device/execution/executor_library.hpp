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
     * loaded, and the device core's own tests bind contexts with none. */
    RvkExecutor         *create(SubApi api, const uint8_t *gpu_uuid) const;
    /* Only of an executor create answered. */
    void                 destroy(RvkExecutor *executor) const;
    uint32_t             execute(RvkExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &reply_size) const;
    /* Whether this library carries version 2's memory channel at all. A
     * version 1 library is a library; it just imports nothing. */
    bool                 imports() const;
    /* Which RVK_BLOB_* kinds `api` imports on the GPU `gpu_uuid` names, or 0.
     * Asked on the main loop while the capset is being described. */
    uint32_t             import_kinds(SubApi api, const uint8_t *gpu_uuid) const;
    /* Whether the library answers the wire handle an import made (version 3),
     * without which a guest has nothing to name an import by. */
    bool                 names_imports() const;
    /* Only on a library that names imports. */
    uint32_t             import_blob(RvkExecutor *executor, uint64_t blob_id, const RvkBlobHandle &handle) const;
    void                 release_blob(RvkExecutor *executor, uint64_t blob_id) const;
    uint64_t             imported_handle(RvkExecutor *executor, uint64_t blob_id) const;

private:
    static constexpr uint32_t known_sub_apis = 7u;
    /* RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF: a kind this device does not
     * know is a kind it cannot build a handle for, so it is not offered. */
    static constexpr uint32_t known_blob_kinds = 3u;

    const RvkExecutorOps *ops_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LIBRARY_HPP
