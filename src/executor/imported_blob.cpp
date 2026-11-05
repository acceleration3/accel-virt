#include "executor/imported_blob.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ImportedBlob::ImportedBlob() : id_(0u), memory_(nullptr), handle_(0u), arena_(false)
{
    AV_SCOPE("ImportedBlob::ImportedBlob");
}

Status ImportedBlob::take(uint64_t blob_id, SourceMemory &memory)
{
    AV_SCOPE("ImportedBlob::take");
    if (memory_ != nullptr) return Status::invalid_state;
    id_ = blob_id;
    memory_ = &memory;
    handle_ = 0u;
    arena_ = false;
    return Status::ok;
}

SourceMemory *ImportedBlob::give_back()
{
    AV_SCOPE("ImportedBlob::give_back");
    SourceMemory *const held = memory_;
    id_ = 0u;
    memory_ = nullptr;
    handle_ = 0u;
    arena_ = false;
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

SourceMemory &ImportedBlob::memory() const
{
    AV_SCOPE("ImportedBlob::memory");
    return *memory_;
}

uint64_t ImportedBlob::handle() const
{
    AV_SCOPE("ImportedBlob::handle");
    return handle_;
}

bool ImportedBlob::arena() const
{
    AV_SCOPE("ImportedBlob::arena");
    return arena_;
}

void ImportedBlob::keep_as_arena()
{
    AV_SCOPE("ImportedBlob::keep_as_arena");
    arena_ = memory_ != nullptr;
}

void ImportedBlob::record(uint64_t handle)
{
    AV_SCOPE("ImportedBlob::record");
    handle_ = handle;
}

bool ImportedBlob::invariants_hold() const
{
    AV_SCOPE("ImportedBlob::invariants_hold");
    return (memory_ != nullptr || (id_ == 0u && handle_ == 0u)) && (memory_ != nullptr || !arena_);
}

}
