#include "device/executor/import_table.hpp"

namespace av {

ImportTable::ImportTable() : blobs_{}, count_(0u) {}

Status ImportTable::take(uint64_t blob_id, const RvkBlobHandle &handle, ImportedBlob *&taken)
{
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
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (blobs_[slot].holds(blob_id)) return &blobs_[slot];
    }
    return nullptr;
}

ImportedBlob *ImportTable::drop(uint64_t blob_id)
{
    ImportedBlob *const blob = find(blob_id);
    if (blob == nullptr) return nullptr;
    count_--;
    return blob;
}

ImportedBlob *ImportTable::any()
{
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (blobs_[slot].live()) return &blobs_[slot];
    }
    return nullptr;
}

uint32_t ImportTable::count() const { return count_; }

bool ImportTable::invariants_hold() const
{
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_imports; slot++) {
        if (!blobs_[slot].invariants_hold()) return false;
        if (blobs_[slot].live()) held++;
    }
    return held == count_;
}

}  // namespace av
