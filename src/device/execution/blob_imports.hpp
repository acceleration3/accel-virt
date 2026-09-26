/* One context's imports of its chunks into its executor (feature 045). A
 * chunk is a guest blob, or a host blob the driver placed in the host window
 * (docs/host-window.md). A host API imports one contiguous range, so each
 * import holds its blob - a guest blob's alias, one host range over its pages
 * that the blob table makes for the first holder and lets go after the last,
 * or a host blob's own mapping - for as long as the import lives. The blob is
 * not released while held, so an id is never re-created under an import
 * still using its old memory.
 *
 * The hold is taken on the worker, under the blob table's lock, and not on the
 * main loop: a worker waiting for the main loop would deadlock against a
 * context being destroyed there, which joins the worker before anything it
 * posted could run. The table of imports is allocated by the first import, so
 * a context that never imports costs a pointer. Worker, except configure. */
#ifndef AV_BLOB_IMPORTS_HPP
#define AV_BLOB_IMPORTS_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"

#include <stdint.h>

namespace av {

class BlobTable;
class ContextExecutor;
class HeldBlob;
class Host;
class WireRecorder;
struct BlobImportsProbe;

class BlobImports {
public:
    /* What the executor library keeps for one context (its ImportTable). */
    static constexpr uint32_t max_imports = 256u;

    BlobImports(Host &host, BlobTable &blobs, ContextExecutor &executor, WireRecorder &recorder,
                uint32_t context);
    /* Main loop, after the worker destroyed the executor, which released every
     * import: the blobs they held are let go. */
    ~BlobImports();
    BlobImports(const BlobImports &) = delete;
    BlobImports &operator=(const BlobImports &) = delete;

    /* Main loop, at setup, before the worker reads the ring: the context's
     * resources begin at its ring. */
    void                 configure(uint32_t ring_resource);
    /* Worker. `size` bytes at `offset` of chunk `resource` imported as memory
     * type `memory_type`, with `device_memory` the handle the import became.
     * not_found for a resource that is not a blob in one of this context's
     * chunk slots; too_large for a range past the blob; no_memory when its
     * pages cannot be aliased or a host blob has no mapping; not_supported
     * when the executor does
     * not make the import; no_space when the context holds max_imports or the
     * host has no table for them. Whole pages, which the caller judged.
     * Nothing new is held unless ok. */
    [[nodiscard]] Status import(uint32_t resource, uint64_t offset, uint64_t size, uint32_t memory_type,
                                uint64_t &device_memory);
    /* Worker. The import `device_memory` names, if this context made one. */
    void                 release(uint64_t device_memory);
    uint32_t             imports() const;

    bool invariants_hold() const;

private:
    /* The slot holding the import `device_memory` names - a free one for 0 -
     * or max_imports. */
    uint32_t             slot_of(uint64_t device_memory) const;
    /* The table, allocated by the first import: no_space when the host has none. */
    [[nodiscard]] Status reserve();
    /* `size` bytes at `offset` of the held blob handed to the executor under
     * `blob_id`: as a dma-buf where the host and the library both take one,
     * and as a host pointer into the held range otherwise. The dma-buf is
     * tried first because a host GPU that refuses a userptr over file-backed
     * pages - and guest RAM and a host blob's memfd are both file-backed -
     * takes only that one. Both attempts are the same import and so carry
     * the same id; the refused one left the library holding nothing under it. */
    [[nodiscard]] Status hand_over(const HeldBlob &held, uint64_t offset, uint64_t size, uint32_t memory_type,
                                   uint64_t blob_id, uint32_t slot);
    [[nodiscard]] Status hand_over_dmabuf(const HeldBlob &held, uint64_t offset, uint64_t size,
                                          uint32_t memory_type, uint64_t blob_id, uint32_t slot);
    [[nodiscard]] Status hand_over_pointer(const HeldBlob &held, uint64_t offset, uint64_t size,
                                           uint32_t memory_type, uint64_t blob_id, uint32_t slot);
    /* The executor given one handle under `blob_id`, with what it made kept. */
    [[nodiscard]] Status offer(const RvkBlobHandle &handle, uint64_t blob_id, uint32_t slot);

    Host            &host_;
    BlobTable       &blobs_;
    ContextExecutor &executor_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    uint32_t         ring_resource_;
    /* Never reused while the context lives, as the executor ABI requires. */
    uint64_t         next_blob_id_;
    /* One host block: each import's handle (0 for a free slot, since no import
     * is named 0), the id the executor holds it under, and its resource. */
    uint64_t        *device_memory_;
    uint64_t        *blob_ids_;
    uint32_t        *resources_;
    /* The pointer fallback was logged: once per context is enough. */
    bool             pointer_noted_;

    friend struct BlobImportsProbe;
};

}  // namespace av

#endif  // AV_BLOB_IMPORTS_HPP
