#include "device/execution/blob_imports.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/context_executor.hpp"
#include "device/host/host.hpp"
#include "device/memory/host_blob_owners.hpp"
#include "device/recording/wire_recorder.hpp"

namespace av {

BlobImports::BlobImports(Host &host, HostBlobOwners &owners, ContextExecutor &executor, WireRecorder &recorder,
                         uint32_t context)
    : host_(host), owners_(owners), executor_(executor), recorder_(recorder), context_(context), next_blob_id_(1u),
      device_memory_(nullptr), blob_ids_(nullptr)
{
    AV_SCOPE("BlobImports::BlobImports");
}

BlobImports::~BlobImports()
{
    AV_SCOPE("BlobImports::~BlobImports");
    if (device_memory_ != nullptr) host_.release(device_memory_);
}

Status BlobImports::reserve()
{
    AV_SCOPE("BlobImports::reserve");
    if (device_memory_ != nullptr) return Status::ok;
    void *const block = host_.allocate(max_imports * sizeof(uint64_t) * 2u);
    if (block == nullptr) return Status::no_space;
    device_memory_ = static_cast<uint64_t *>(block);
    blob_ids_ = device_memory_ + max_imports;
    return Status::ok;
}

/* The executor names the memory by the blob's resource id, which is the key
 * the device allocated it under (HostBlobTable::create). */
Status BlobImports::import(uint32_t resource, uint64_t &device_memory)
{
    AV_SCOPE("BlobImports::import");
    host_.require_worker("BlobImports::import");
    device_memory = 0u;
    if (!owners_.owns(context_, resource)) return Status::not_found;
    const uint32_t slot = reserve() == Status::ok ? slot_of(0u) : max_imports;
    if (slot == max_imports) return Status::no_space;
    const uint64_t blob_id = next_blob_id_++;
    uint64_t       made = 0u;
    if (executor_.import(blob_id, resource, made) != Status::ok) return Status::not_supported;
    /* Two imports answering to one handle would make RELEASE_BLOB of either
     * release both: a library that says so has broken the ABI. */
    if (slot_of(made) != max_imports) {
        executor_.release(blob_id);
        return Status::not_supported;
    }
    device_memory_[slot] = made;
    blob_ids_[slot] = blob_id;
    device_memory = made;
    recorder_.record_blob(context_, BlobAction::imported, 0u, blob_id, 0u, resource);
    return Status::ok;
}

void BlobImports::release(uint64_t device_memory)
{
    AV_SCOPE("BlobImports::release");
    host_.require_worker("BlobImports::release");
    const uint32_t slot = device_memory != 0u ? slot_of(device_memory) : max_imports;
    if (slot == max_imports) return;
    executor_.release(blob_ids_[slot]);
    recorder_.record_blob(context_, BlobAction::released, 0u, blob_ids_[slot], 0u, 0u);
    device_memory_[slot] = 0u;
}

uint32_t BlobImports::slot_of(uint64_t device_memory) const
{
    AV_SCOPE("BlobImports::slot_of");
    for (uint32_t slot = 0u; device_memory_ != nullptr && slot < max_imports; slot++)
        if (device_memory_[slot] == device_memory) return slot;
    return max_imports;
}

uint32_t BlobImports::imports() const
{
    AV_SCOPE("BlobImports::imports");
    uint32_t count = 0u;
    for (uint32_t slot = 0u; device_memory_ != nullptr && slot < max_imports; slot++)
        count += device_memory_[slot] != 0u ? 1u : 0u;
    return count;
}

/* Every import is named by a handle no other import answers to, and no import
 * was made under an id not yet handed out. */
bool BlobImports::invariants_hold() const
{
    AV_SCOPE("BlobImports::invariants_hold");
    for (uint32_t slot = 0u; device_memory_ != nullptr && slot < max_imports; slot++) {
        if (device_memory_[slot] == 0u) continue;
        if (blob_ids_[slot] == 0u || blob_ids_[slot] >= next_blob_id_) return false;
        for (uint32_t other = slot + 1u; other < max_imports; other++)
            if (device_memory_[other] == device_memory_[slot]) return false;
    }
    return true;
}

}  // namespace av
