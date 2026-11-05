#ifndef AV_BLOB_IMPORTS_HPP
#define AV_BLOB_IMPORTS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "proto/submission/blob_role.hpp"

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

    [[nodiscard]] Status import(uint32_t resource, uint64_t &device_memory, const BlobRole &role = BlobRole());
    void                 release(uint64_t device_memory);
    uint32_t             imports() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    HostBlobOwners  &owners_;
    ContextExecutor &executor_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    uint64_t         next_blob_id_;
    KeyedSlots<uint64_t, uint64_t> blob_ids_;

    friend struct BlobImportsProbe;
};

}

#endif
