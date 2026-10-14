/* The blobs one session holds, by the id the guest used.
 *
 * Named a table of imports rather than of blobs because the device already has
 * an `av::BlobTable` for its own blob resources, and these are different
 * things: that one is what the guest created, this one is what a host API was
 * handed. */
#ifndef AV_IMPORT_TABLE_HPP
#define AV_IMPORT_TABLE_HPP

#include "core/support/status.hpp"
#include "executor/imported_blob.hpp"

#include <stdint.h>


namespace av {

class SourceMemory;

class ImportTable {
public:
    /* As many as the device lets one context hold (BlobImports). */
    static constexpr uint32_t max_imports = 4096u;

    ImportTable();
    ImportTable(const ImportTable &) = delete;
    ImportTable &operator=(const ImportTable &) = delete;

    /* Records `memory`, held for the import, under `blob_id`. conflict for an
     * id already imported - a silent teardown is what a hostile guest would
     * drive at the worst moment - and no_space when the table is full. */
    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken);
    /* The blob `blob_id` names, or null. */
    ImportedBlob        *find(uint64_t blob_id);
    /* Gives one back, or nothing if that id was never imported: release is
     * idempotent, because the device may release an id whose import failed. */
    ImportedBlob        *drop(uint64_t blob_id);
    /* Every blob still held, so a session being destroyed releases what the
     * guest never did. Null when there is nothing left. */
    ImportedBlob        *any();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    ImportedBlob blobs_[max_imports];
    uint32_t     count_;
};

}  // namespace av

#endif  // AV_IMPORT_TABLE_HPP
