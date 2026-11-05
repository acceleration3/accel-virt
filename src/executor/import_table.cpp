#include "executor/import_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ImportTable::ImportTable(BlockSource &blocks)
    : blobs_(blocks), ids_(blocks, Pool::paged), handles_(blocks, Pool::paged)
{
    AV_SCOPE("ImportTable::ImportTable");
}

Status ImportTable::take(uint64_t blob_id, SourceMemory &memory, ImportedBlob *&taken)
{
    AV_SCOPE("ImportTable::take");
    taken = nullptr;
    if (ids_.find(blob_id) != nullptr) return Status::conflict;
    Status room = ids_.reserve(ids_.count() + 1u);
    if (room == Status::ok) room = handles_.reserve(ids_.count() + 1u);
    if (room != Status::ok) return room;
    ImportedBlob *made = nullptr;
    const Status  created = blobs_.create(made);
    if (created != Status::ok) return created;
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

Status ImportTable::name(ImportedBlob &blob, uint64_t handle)
{
    AV_SCOPE("ImportTable::name");
    if (handle == 0u || blob.handle() != 0u) return Status::invalid_parameter;
    uint32_t     slot = 0u;
    const Status named = handles_.insert(handle, &blob, slot);
    if (named != Status::ok) return named;
    blob.record(handle);
    return Status::ok;
}

SourceMemory *ImportTable::named(uint64_t handle) const
{
    AV_SCOPE("ImportTable::named");
    if (handle == 0u) return nullptr;
    ImportedBlob *const *const found = handles_.find(handle);
    return found != nullptr ? &(*found)->memory() : nullptr;
}

void ImportTable::unname(uint64_t handle)
{
    AV_SCOPE("ImportTable::unname");
    ImportedBlob *blob = nullptr;
    if (handle == 0u || !handles_.take(handle, blob)) return;
    blob->record(0u);
}

SourceMemory *ImportTable::forget(uint64_t blob_id)
{
    AV_SCOPE("ImportTable::forget");
    ImportedBlob *blob = nullptr;
    if (!ids_.take(blob_id, blob)) return nullptr;
    unname(blob->handle());
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
    bool     holds = blobs_.live() == ids_.count() && handles_.count() <= ids_.count();
    uint32_t named = 0u;
    ids_.for_each([&holds, &named, this](uint64_t id, ImportedBlob *const &blob) {
        holds = holds && blob->invariants_hold() && blob->holds(id);
        if (blob->handle() == 0u) return;
        named++;
        ImportedBlob *const *const under = handles_.find(blob->handle());
        holds = holds && under != nullptr && *under == blob;
    });
    return holds && named == handles_.count();
}

}
