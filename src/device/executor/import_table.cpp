#include "device/executor/import_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ImportTable::ImportTable() : blobs_{}, count_(0u)
{
    AV_SCOPE("ImportTable::ImportTable");
}

Status ImportTable::take(uint64_t blob_id, const RvkBlobHandle &handle, ImportedBlob *&taken)
{
    AV_SCOPE("ImportTable::take");
    taken = nullptr;
    if (find(blob_id) != nullptr) return Status::conflict;
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (blobs_[slot].live()) continue;
        const Status status = blobs_[slot].take(blob_id, handle);
        if (status != Status::ok) return status;
        count_++;
        taken = &blobs_[slot];
        return Status::ok;
    }
    return Status::no_space;
}

ImportedBlob *ImportTable::find(uint64_t blob_id)
{
    AV_SCOPE("ImportTable::find");
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (blobs_[slot].holds(blob_id)) return &blobs_[slot];
    }
    return nullptr;
}

ImportedBlob *ImportTable::drop(uint64_t blob_id)
{
    AV_SCOPE("ImportTable::drop");
    ImportedBlob *const blob = find(blob_id);
    if (blob == nullptr) return nullptr;
    count_--;
    return blob;
}

ImportedBlob *ImportTable::any()
{
    AV_SCOPE("ImportTable::any");
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (blobs_[slot].live()) return &blobs_[slot];
    }
    return nullptr;
}

uint32_t ImportTable::count() const
{
    AV_SCOPE("ImportTable::count");
    return count_;
}

bool ImportTable::invariants_hold() const
{
    AV_SCOPE("ImportTable::invariants_hold");
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (!blobs_[slot].invariants_hold()) return false;
        if (blobs_[slot].live()) held++;
    }
    return held == count_;
}

}  // namespace av
