#include "executor/import_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ImportTable::ImportTable(BlockSource &blocks) : blobs_(blocks), ids_(blocks, Pool::paged)
{
    AV_SCOPE("ImportTable::ImportTable");
}

Status ImportTable::take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken)
{
    AV_SCOPE("ImportTable::take");
    taken = nullptr;
    if (ids_.find(blob_id) != nullptr) return Status::conflict;
    const Status room = ids_.reserve(ids_.count() + 1u);
    if (room != Status::ok) return room;
    ImportedBlob *made = nullptr;
    const Status  created = blobs_.create(made);
    if (created != Status::ok) return created;
    /* A blob just made holds nothing, so it takes; the room was made above. */
    static_cast<void>(made->take(blob_id, memory));
    uint32_t slot = 0u;
    static_cast<void>(ids_.insert(blob_id, made, slot));
    taken = made;
    return Status::ok;
}

ImportedBlob *ImportTable::find(uint64_t blob_id)
{
    AV_SCOPE("ImportTable::find");
    ImportedBlob *const *const found = ids_.find(blob_id);
    return found != nullptr ? *found : nullptr;
}

SourceMemory *ImportTable::forget(uint64_t blob_id)
{
    AV_SCOPE("ImportTable::forget");
    ImportedBlob *blob = nullptr;
    if (!ids_.take(blob_id, blob)) return nullptr;
    SourceMemory *const memory = blob->give_back();
    static_cast<void>(blobs_.destroy(blob));
    return memory;
}

ImportedBlob *ImportTable::any()
{
    AV_SCOPE("ImportTable::any");
    const uint32_t slot = ids_.next_live(0u);
    return slot != KeyedSlots<uint64_t, ImportedBlob *>::no_slot ? *ids_.at(slot) : nullptr;
}

uint32_t ImportTable::count() const
{
    AV_SCOPE("ImportTable::count");
    return ids_.count();
}

bool ImportTable::invariants_hold() const
{
    AV_SCOPE("ImportTable::invariants_hold");
    bool holds = blobs_.live() == ids_.count();
    ids_.for_each([&holds](uint64_t id, ImportedBlob *const &blob) {
        holds = holds && blob->invariants_hold() && blob->holds(id);
    });
    return holds;
}

}  // namespace av
