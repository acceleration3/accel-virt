#include "executor/imported_memories.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"

namespace av {

ImportedMemories::ImportedMemories() : memories_{}, count_(0u)
{
    AV_SCOPE("ImportedMemories::ImportedMemories");
}

bool ImportedMemories::adopt(ImportedBlob &blob)
{
    AV_SCOPE("ImportedMemories::adopt");
    for (const SourceMemory *&slot : memories_) {
        if (slot != nullptr) continue;
        slot = &blob.memory();
        count_++;
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(slot));
        blob.record(handle, handle);
        return true;
    }
    return false;
}

void ImportedMemories::abandon(ImportedBlob &blob)
{
    AV_SCOPE("ImportedMemories::abandon");
    for (const SourceMemory *&slot : memories_) {
        if (slot != &blob.memory()) continue;
        slot = nullptr;
        count_--;
        blob.record(0u, 0u);
        return;
    }
}

const SourceMemory *ImportedMemories::find(uint64_t handle) const
{
    AV_SCOPE("ImportedMemories::find");
    if (handle == 0u) return nullptr;
    for (const SourceMemory *const slot : memories_)
        if (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(slot)) == handle) return slot;
    return nullptr;
}

void ImportedMemories::forget_all()
{
    AV_SCOPE("ImportedMemories::forget_all");
    for (const SourceMemory *&slot : memories_) slot = nullptr;
    count_ = 0u;
}

uint32_t ImportedMemories::count() const
{
    AV_SCOPE("ImportedMemories::count");
    return count_;
}

bool ImportedMemories::invariants_hold() const
{
    AV_SCOPE("ImportedMemories::invariants_hold");
    uint32_t held = 0u;
    for (const SourceMemory *const slot : memories_) held += slot != nullptr ? 1u : 0u;
    return held == count_;
}

}  // namespace av
