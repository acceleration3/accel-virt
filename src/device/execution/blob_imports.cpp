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
      blob_ids_(host, Pool::paged)
{
    AV_SCOPE("BlobImports::BlobImports");
}

BlobImports::~BlobImports()
{
    AV_SCOPE("BlobImports::~BlobImports");
}

Status BlobImports::import(uint32_t resource, uint64_t &device_memory)
{
    AV_SCOPE("BlobImports::import");
    host_.require_worker("BlobImports::import");
    device_memory = 0u;
    if (!owners_.owns(context_, resource)) return Status::not_found;
    if (blob_ids_.reserve(blob_ids_.count() + 1u) != Status::ok) return Status::no_memory;
    const uint64_t blob_id = next_blob_id_++;
    uint64_t       made = 0u;
    if (executor_.import(blob_id, resource, made) != Status::ok) return Status::not_supported;
    uint32_t slot = 0u;
    if (made == 0u || blob_ids_.insert(made, blob_id, slot) != Status::ok) {
        executor_.release(blob_id);
        return Status::not_supported;
    }
    device_memory = made;
    recorder_.record_blob(context_, BlobAction::imported, 0u, blob_id, 0u, resource);
    return Status::ok;
}

void BlobImports::release(uint64_t device_memory)
{
    AV_SCOPE("BlobImports::release");
    host_.require_worker("BlobImports::release");
    uint64_t blob_id = 0u;
    if (!blob_ids_.take(device_memory, blob_id)) return;
    executor_.release(blob_id);
    recorder_.record_blob(context_, BlobAction::released, 0u, blob_id, 0u, 0u);
}

uint32_t BlobImports::imports() const
{
    AV_SCOPE("BlobImports::imports");
    return blob_ids_.count();
}

bool BlobImports::invariants_hold() const
{
    AV_SCOPE("BlobImports::invariants_hold");
    bool held = blob_ids_.invariants_hold();
    blob_ids_.for_each([this, &held](uint64_t device_memory, uint64_t blob_id) {
        held = held && device_memory != 0u && blob_id != 0u && blob_id < next_blob_id_;
    });
    return held;
}

}
