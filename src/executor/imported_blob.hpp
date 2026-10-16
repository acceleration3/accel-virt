/* One import a session holds: memory of its GPU's memory source
 * (docs/host-memory.md), named by the id the device gave the import, and the
 * number the guest names what the session's host API made of it by. The
 * session holds a reference to the memory for as long as the import lives. */
#ifndef AV_IMPORTED_BLOB_HPP
#define AV_IMPORTED_BLOB_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class SourceMemory;

class ImportedBlob {
public:
    ImportedBlob();
    ImportedBlob(const ImportedBlob &) = delete;
    ImportedBlob &operator=(const ImportedBlob &) = delete;

    /* `memory`, already held for the import, under `blob_id`: invalid_state
     * for a slot holding one. */
    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory);
    /* The slot given back, and the memory it held answered for its holder to
     * let go of; null for a slot holding nothing. */
    SourceMemory        *give_back();

    bool                holds(uint64_t blob_id) const;
    bool                live() const;
    uint64_t            id() const;
    /* What is imported; only on a live slot. */
    const SourceMemory &memory() const;
    /* What the host API made of it - a VkDeviceMemory, a heap's memory, a GL
     * memory object - as the number the guest names it by in that API's
     * commands; 0 while it is named by nothing: before the backend adopted
     * it, and after the guest freed what was made. */
    uint64_t            handle() const;

    bool invariants_hold() const;

private:
    friend class ImportTable;
    friend struct ImportedBlobProbe;

    /* Only ImportTable names a blob, since it keeps the index by name. */
    void record(uint64_t handle);

    uint64_t      id_;
    SourceMemory *memory_;
    uint64_t      handle_;
};

}  // namespace av

#endif  // AV_IMPORTED_BLOB_HPP
