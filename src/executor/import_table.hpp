/* The blobs one session holds, by the id the guest used.
 *
 * Named a table of imports rather than of blobs because the device already has
 * an `av::BlobTable` for its own blob resources, and these are different
 * things: that one is what the guest created, this one is what a host API was
 * handed. */
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
    /* As many as the device lets one context hold; an import stays where it
     * was made until it is forgotten, so a backend may keep its address. */
    explicit ImportTable(BlockSource &blocks);
    ImportTable(const ImportTable &) = delete;
    ImportTable &operator=(const ImportTable &) = delete;

    /* Records `memory`, held for the import, under `blob_id`. conflict for an
     * id already imported - a silent teardown is what a hostile guest would
     * drive at the worst moment - and no_memory when the table cannot grow. */
    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken);
    /* The blob `blob_id` names, or null. */
    ImportedBlob        *find(uint64_t blob_id);
    /* The import gone, and the memory it held answered for its holder to let
     * go of; null for an id never imported: release is idempotent, because
     * the device may release an id whose import failed. Its backend is told
     * first. */
    SourceMemory        *forget(uint64_t blob_id);
    /* Every blob still held, so a session being destroyed releases what the
     * guest never did. Null when there is nothing left. */
    ImportedBlob        *any();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    ObjectPool<ImportedBlob>             blobs_;
    KeyedSlots<uint64_t, ImportedBlob *> ids_;
};

}  // namespace av

#endif  // AV_IMPORT_TABLE_HPP
