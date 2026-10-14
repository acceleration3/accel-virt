/* One context's imports of host blobs into its executor (docs/host-memory.md).
 * A host blob is host GPU memory the executor library allocated when the
 * context's process opened it; an import names that memory to the context's
 * host API. Only a blob made for this context is imported. The library keeps
 * the memory for as long as the import holds it, whatever the guest releases
 * meanwhile, so nothing here holds the blob itself.
 *
 * The table of imports is allocated by the first import, so a context that
 * never imports costs a pointer. Worker. */
#ifndef AV_BLOB_IMPORTS_HPP
#define AV_BLOB_IMPORTS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ContextExecutor;
class Host;
class HostBlobOwners;
class WireRecorder;
struct BlobImportsProbe;

class BlobImports {
public:
    /* What the executor library keeps for one context (its ImportTable). */
    static constexpr uint32_t max_imports = 4096u;

    BlobImports(Host &host, HostBlobOwners &owners, ContextExecutor &executor, WireRecorder &recorder,
                uint32_t context);
    ~BlobImports();
    BlobImports(const BlobImports &) = delete;
    BlobImports &operator=(const BlobImports &) = delete;

    /* Worker. Host blob `resource` imported whole, with `device_memory` the
     * handle the import became. not_found for a resource that is no host
     * blob made for this context; not_supported when the executor does not
     * make the import; no_space when the context holds max_imports or the
     * host has no table for them. Nothing new is held unless ok. */
    [[nodiscard]] Status import(uint32_t resource, uint64_t &device_memory);
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

    Host            &host_;
    HostBlobOwners  &owners_;
    ContextExecutor &executor_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    /* Never reused while the context lives, as the executor ABI requires. */
    uint64_t         next_blob_id_;
    /* One host block: each import's handle (0 for a free slot, since no import
     * is named 0) and the id the executor holds it under. */
    uint64_t        *device_memory_;
    uint64_t        *blob_ids_;

    friend struct BlobImportsProbe;
};

}  // namespace av

#endif  // AV_BLOB_IMPORTS_HPP
