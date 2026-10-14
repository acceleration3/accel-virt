#include "executor/imported_blob.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ImportedBlob::ImportedBlob() : id_(0u), memory_(nullptr), object_(0u), wire_handle_(0u)
{
    AV_SCOPE("ImportedBlob::ImportedBlob");
}

Status ImportedBlob::take(uint64_t blob_id, SourceMemory &memory)
{
    AV_SCOPE("ImportedBlob::take");
    if (memory_ != nullptr) return Status::invalid_state;
    id_ = blob_id;
    memory_ = &memory;
    object_ = 0u;
    wire_handle_ = 0u;
    return Status::ok;
}

SourceMemory *ImportedBlob::give_back()
{
    AV_SCOPE("ImportedBlob::give_back");
    SourceMemory *const held = memory_;
    id_ = 0u;
    memory_ = nullptr;
    object_ = 0u;
    wire_handle_ = 0u;
    return held;
}

bool ImportedBlob::holds(uint64_t blob_id) const
{
    AV_SCOPE("ImportedBlob::holds");
    return memory_ != nullptr && id_ == blob_id;
}

bool ImportedBlob::live() const
{
    AV_SCOPE("ImportedBlob::live");
    return memory_ != nullptr;
}

uint64_t ImportedBlob::id() const
{
    AV_SCOPE("ImportedBlob::id");
    return id_;
}

const SourceMemory &ImportedBlob::memory() const
{
    AV_SCOPE("ImportedBlob::memory");
    return *memory_;
}

void ImportedBlob::record(uint64_t object, uint64_t wire_handle)
{
    AV_SCOPE("ImportedBlob::record");
    object_ = object;
    wire_handle_ = wire_handle;
}

uint64_t ImportedBlob::object() const
{
    AV_SCOPE("ImportedBlob::object");
    return object_;
}

uint64_t ImportedBlob::wire_handle() const
{
    AV_SCOPE("ImportedBlob::wire_handle");
    return wire_handle_;
}

/* A free slot is all zeros: nothing of a previous import may look live. */
bool ImportedBlob::invariants_hold() const
{
    AV_SCOPE("ImportedBlob::invariants_hold");
    return memory_ != nullptr || (id_ == 0u && object_ == 0u && wire_handle_ == 0u);
}

}  // namespace av
