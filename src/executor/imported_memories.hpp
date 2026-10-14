/* The memory a session imported besides its region (docs/host-memory.md):
 * each a host allocation the guest opened for one object - a Direct3D 12
 * heap, a Direct3D 11 slice store - and names in that object's command by the
 * handle its import was answered with. The handle is the memory's own
 * address, which no two live imports share. */
#ifndef AV_IMPORTED_MEMORIES_HPP
#define AV_IMPORTED_MEMORIES_HPP

#include "executor/import_table.hpp"

#include <stdint.h>

namespace av {

class ImportedBlob;
class SourceMemory;
struct ImportedMemoriesProbe;

class ImportedMemories {
public:
    static constexpr uint32_t capacity = ImportTable::max_imports;

    ImportedMemories();
    ImportedMemories(const ImportedMemories &) = delete;
    ImportedMemories &operator=(const ImportedMemories &) = delete;

    /* `blob`'s memory kept, and the handle it is named by recorded on the
     * blob: false, recording nothing, when full. */
    [[nodiscard]] bool adopt(ImportedBlob &blob);
    /* What adopt kept of `blob`, forgotten; nothing for a blob it did not. */
    void               abandon(ImportedBlob &blob);
    /* The memory named `handle`, or null. */
    const SourceMemory *find(uint64_t handle) const;
    void                forget_all();
    uint32_t            count() const;

    bool invariants_hold() const;

private:
    const SourceMemory *memories_[capacity];
    uint32_t            count_;

    friend struct ImportedMemoriesProbe;
};

}  // namespace av

#endif  // AV_IMPORTED_MEMORIES_HPP
