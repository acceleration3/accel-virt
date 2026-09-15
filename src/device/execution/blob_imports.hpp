/* One context's imports of its guest chunks into its executor (feature 045).
 * A host API imports a pointer, so each import holds its blob's alias - one
 * host range over the blob's pages, which the blob table makes for the first
 * holder and lets go after the last - for as long as the import lives. The
 * blob is not released while held, so an id is never re-created under an
 * import still using its old pages.
 *
 * The alias is made on the worker, under the blob table's lock, and not on the
 * main loop: a worker waiting for the main loop would deadlock against a
 * context being destroyed there, which joins the worker before anything it
 * posted could run. The table of imports is allocated by the first import, so
 * a context that never imports costs a pointer. Worker, except configure. */
#ifndef AV_BLOB_IMPORTS_HPP
#define AV_BLOB_IMPORTS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class ContextExecutor;
class GuestPageAlias;
class Host;
struct BlobImportsProbe;

class BlobImports {
public:
    /* What the executor library keeps for one context (its ImportTable). */
    static constexpr uint32_t max_imports = 256u;

    BlobImports(Host &host, BlobTable &blobs, ContextExecutor &executor);
    /* Main loop, after the worker destroyed the executor, which released every
     * import: the aliases they held are let go. */
    ~BlobImports();
    BlobImports(const BlobImports &) = delete;
    BlobImports &operator=(const BlobImports &) = delete;

    /* Main loop, at setup, before the worker reads the ring: the context's
     * resources begin at its ring. */
    void                 configure(uint32_t ring_resource);
    /* Worker. `size` bytes at `offset` of chunk `resource` imported as memory
     * type `memory_type`, with `device_memory` the handle the import became.
     * not_found for a resource that is not a guest blob in one of this
     * context's chunk slots; too_large for a range past the blob; no_memory
     * when its pages cannot be aliased; not_supported when the executor does
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
    /* `size` bytes at `offset` of the held alias handed to the executor. */
    [[nodiscard]] Status hand_over(const GuestPageAlias &alias, uint64_t offset, uint64_t size,
                                   uint32_t memory_type, uint32_t slot);

    Host            &host_;
    BlobTable       &blobs_;
    ContextExecutor &executor_;
    uint32_t         ring_resource_;
    /* Never reused while the context lives, as the executor ABI requires. */
    uint64_t         next_blob_id_;
    /* One host block: each import's handle (0 for a free slot, since no import
     * is named 0), the id the executor holds it under, and its resource. */
    uint64_t        *device_memory_;
    uint64_t        *blob_ids_;
    uint32_t        *resources_;

    friend struct BlobImportsProbe;
};

}  // namespace av

#endif  // AV_BLOB_IMPORTS_HPP
