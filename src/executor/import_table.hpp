#ifndef AV_IMPORT_TABLE_HPP
#define AV_IMPORT_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/imported_blob.hpp"

#include <stdint.h>

namespace av {

class SourceMemory;

class ImportTable {
public:
    explicit ImportTable(BlockSource &blocks);
    ImportTable(const ImportTable &) = delete;
    ImportTable &operator=(const ImportTable &) = delete;

    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken);
    ImportedBlob        *find(uint64_t blob_id);
    [[nodiscard]] Status name(ImportedBlob &blob, uint64_t handle);
    SourceMemory        *named(uint64_t handle) const;
    void                 unname(uint64_t handle);
    SourceMemory        *forget(uint64_t blob_id);
    ImportedBlob        *any();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    ObjectPool<ImportedBlob>             blobs_;
    KeyedSlots<uint64_t, ImportedBlob *> ids_;
    KeyedSlots<uint64_t, ImportedBlob *> handles_;
};

}

#endif
