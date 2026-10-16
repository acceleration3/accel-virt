/* One context's imports of host blobs into its executor (docs/host-memory.md).
 * A host blob is host GPU memory the executor library allocated when the
 * context's process opened it; an import names that memory to the context's
 * host API. Only a blob made for this context is imported. The library keeps
 * the memory for as long as the import holds it, whatever the guest releases
 * meanwhile, so nothing here holds the blob itself.
 *
 * The imports are kept by the handle each became, in host memory that grows
 * with them: a context imports as many blobs as its process opens. Nothing is
 * allocated before the first, so a context that never imports costs nothing.
 * Worker. */
#ifndef AV_BLOB_IMPORTS_HPP
#define AV_BLOB_IMPORTS_HPP

#include "core/support/keyed_slots.hpp"
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
    BlobImports(Host &host, HostBlobOwners &owners, ContextExecutor &executor, WireRecorder &recorder,
                uint32_t context);
    ~BlobImports();
    BlobImports(const BlobImports &) = delete;
    BlobImports &operator=(const BlobImports &) = delete;

    /* Worker. Host blob `resource` imported whole, with `device_memory` the
     * handle the import became. not_found for a resource that is no host
     * blob made for this context; not_supported when the executor does not
     * make the import; no_memory when the host has no memory to keep it in,
     * and the executor is not asked then. Nothing new is held unless ok. */
    [[nodiscard]] Status import(uint32_t resource, uint64_t &device_memory);
    /* Worker. The import `device_memory` names, if this context made one. */
    void                 release(uint64_t device_memory);
    uint32_t             imports() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    HostBlobOwners  &owners_;
    ContextExecutor &executor_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    /* Never reused while the context lives, as the executor ABI requires. */
    uint64_t         next_blob_id_;
    /* Each import's id the executor holds it under, by the handle it became. */
    KeyedSlots<uint64_t, uint64_t> blob_ids_;

    friend struct BlobImportsProbe;
};

}  // namespace av

#endif  // AV_BLOB_IMPORTS_HPP
