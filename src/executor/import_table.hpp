/* The blobs one session holds, by the id the device gave each import and by
 * the handle the guest names what its host API made of it by.
 *
 * Named a table of imports rather than of blobs because the device already has
 * an `av::BlobTable` for its own blob resources, and these are different
 * things: that one is what the guest created, this one is what a host API was
 * handed. Every backend finds an import a command names here - Vulkan memory
 * the guest frees, a Direct3D 12 heap's memory, a Direct3D 11 slice store, an
 * OpenCL buffer's lent pages - so none keeps a list of its own. */
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

    /* Records `memory`, held for the import, under `blob_id`, with room made
     * for its name, so that naming it fails only for a handle already taken.
     * conflict for an id already imported - a silent teardown is what a
     * hostile guest would drive at the worst moment - and no_memory when the
     * table cannot grow. */
    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken);
    /* The blob `blob_id` names, or null. */
    ImportedBlob        *find(uint64_t blob_id);
    /* `blob`, named by nothing yet, named `handle` from now on.
     * invalid_parameter for 0 or a blob already named; conflict for a handle
     * another live import is named by, since a command naming it could reach
     * only one of them. */
    [[nodiscard]] Status name(ImportedBlob &blob, uint64_t handle);
    /* The memory of the live import the guest names by `handle`, or null -
     * for 0 as well. Every number a command names came off the wire. */
    const SourceMemory  *named(uint64_t handle) const;
    /* The import `handle` names, named by nothing again and held still: the
     * guest freed what the backend made of it, and the release that follows
     * finds nothing to free. Nothing for a handle that names none. */
    void                 unname(uint64_t handle);
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
    KeyedSlots<uint64_t, ImportedBlob *> handles_;
};

}  // namespace av

#endif  // AV_IMPORT_TABLE_HPP
