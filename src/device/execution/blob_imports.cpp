#include "device/execution/blob_imports.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/context_executor.hpp"
#include "device/host/host.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_dma_buf.hpp"
#include "device/memory/held_blob.hpp"
#include "proto/context/context_resources.hpp"

namespace av {

BlobImports::BlobImports(Host &host, BlobTable &blobs, ContextExecutor &executor, WireRecorder &recorder,
                         uint32_t context)
    : host_(host), blobs_(blobs), executor_(executor), recorder_(recorder), context_(context), ring_resource_(0u), next_blob_id_(1u),
      device_memory_(nullptr), blob_ids_(nullptr), resources_(nullptr), pointer_noted_(false)
      {
          AV_SCOPE("BlobImports::BlobImports");
      }

BlobImports::~BlobImports()
{
    AV_SCOPE("BlobImports::~BlobImports");
    if (device_memory_ == nullptr) return;
    for (uint32_t slot = 0u; slot < max_imports; slot++)
        if (device_memory_[slot] != 0u) blobs_.release_hold(resources_[slot]);
    host_.release(device_memory_);
}

void BlobImports::configure(uint32_t ring_resource)
{
    AV_SCOPE("BlobImports::configure");
    host_.require_main_loop("BlobImports::configure");
    ring_resource_ = ring_resource;
}

Status BlobImports::reserve()
{
    AV_SCOPE("BlobImports::reserve");
    if (device_memory_ != nullptr) return Status::ok;
    void *const block = host_.allocate(max_imports * (sizeof(uint64_t) * 2u + sizeof(uint32_t)));
    if (block == nullptr) return Status::no_space;
    device_memory_ = static_cast<uint64_t *>(block);
    blob_ids_ = device_memory_ + max_imports;
    resources_ = static_cast<uint32_t *>(static_cast<void *>(blob_ids_ + max_imports));
    return Status::ok;
}

Status BlobImports::import(uint32_t resource, uint64_t offset, uint64_t size, uint32_t memory_type,
                           uint64_t &device_memory)
{
    AV_SCOPE("BlobImports::import");
    host_.require_worker("BlobImports::import");
    device_memory = 0u;
    if (ContextResources(ring_resource_).chunk_slot(resource) == ContextResources::chunk_slots)
        return Status::not_found;
    const Status reserved = reserve();
    const uint32_t slot = reserved == Status::ok ? slot_of(0u) : max_imports;
    if (slot == max_imports) return Status::no_space;
    HeldBlob     held;
    const Status holding = blobs_.hold(resource, held);
    /* No blob is the guest naming the wrong resource; anything else is the
     * host failing memory that was fine. */
    if (holding == Status::not_found) return Status::not_found;
    if (holding != Status::ok) return Status::no_memory;
    ScopeExit    unhold([this, resource] { blobs_.release_hold(resource); });
    const Status handed = hand_over(held, offset, size, memory_type, next_blob_id_++, slot);
    if (handed != Status::ok) return handed;
    unhold.dismiss();
    resources_[slot] = resource;
    device_memory = device_memory_[slot];
    recorder_.record_blob(context_, BlobAction::imported, memory_type, blob_ids_[slot], offset, size);
    recorder_.record_blob_bytes(context_, blob_ids_[slot],
                                offset, static_cast<const uint8_t *>(held.base()) + offset, size);
    return Status::ok;
}

Status BlobImports::offer(const AvBlobHandle &handle, uint64_t blob_id, uint32_t slot)
{
    AV_SCOPE("BlobImports::offer");
    uint64_t made = 0u;
    if (executor_.import(blob_id, handle, made) != Status::ok) return Status::not_supported;
    /* Two imports answering to one handle would make RELEASE_BLOB of either
     * release both: a library that says so has broken the ABI. */
    if (slot_of(made) != max_imports) {
        executor_.release(blob_id);
        return Status::not_supported;
    }
    device_memory_[slot] = made;
    blob_ids_[slot] = blob_id;
    return Status::ok;
}

Status BlobImports::hand_over_dmabuf(const HeldBlob &held, uint64_t offset, uint64_t size, uint32_t memory_type,
                                     uint64_t blob_id, uint32_t slot)
{
    AV_SCOPE("BlobImports::hand_over_dmabuf");
    BlobDmaBuf   shared(host_);
    const Status made = held.share(shared, offset, size);
    if (made != Status::ok) return made;
    AvBlobHandle handle = {};
    handle.kind = AV_BLOB_DMA_BUF;
    handle.reserved = memory_type;
    handle.pointer = nullptr;
    handle.fd = shared.handle();
    handle.size = size;
    const Status offered = offer(handle, blob_id, slot);
    /* A library that took it owns the descriptor from then on, as the ABI
     * says; one that refused leaves it here to be closed. */
    if (offered == Status::ok) static_cast<void>(shared.take());
    return offered;
}

Status BlobImports::hand_over_pointer(const HeldBlob &held, uint64_t offset, uint64_t size, uint32_t memory_type,
                                      uint64_t blob_id, uint32_t slot)
{
    AV_SCOPE("BlobImports::hand_over_pointer");
    AvBlobHandle handle = {};
    handle.kind = AV_BLOB_HOST_POINTER;
    handle.reserved = memory_type;
    handle.pointer = static_cast<uint8_t *>(held.base()) + offset;
    handle.fd = -1;
    handle.size = size;
    return offer(handle, blob_id, slot);
}

Status BlobImports::hand_over(const HeldBlob &held, uint64_t offset, uint64_t size, uint32_t memory_type,
                              uint64_t blob_id, uint32_t slot)
{
    AV_SCOPE("BlobImports::hand_over");
    if (size > held.length() || offset > held.length() - size) return Status::too_large;
    /* The dma-buf first: a host GPU that refuses a userptr over file-backed
     * pages takes only that one, and guest RAM and a host blob's memfd are
     * both file-backed. A host with no udmabuf, or a library that imports no
     * dma-buf - OpenGL's and OpenCL's - falls through to the pointer. */
    if (hand_over_dmabuf(held, offset, size, memory_type, blob_id, slot) == Status::ok) return Status::ok;
    /* Said, because a host GPU that refuses the pointer then fails far from
     * here: udmabuf's size_limit_mb and list_limit (64 MiB and 1024 runs by
     * default) are what usually refused it. */
    if (!pointer_noted_) {
        pointer_noted_ = true;
        host_.log("virtio-gpu-av: a blob could not be a dma-buf (raise udmabuf's size_limit_mb and list_limit?); "
                  "handing over a host pointer");
    }
    return hand_over_pointer(held, offset, size, memory_type, blob_id, slot);
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
    blobs_.release_hold(resources_[slot]);
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

/* Every import is one of this context's chunks, named by a handle no other
 * import answers to, and no import was made under an id not yet handed out. */
bool BlobImports::invariants_hold() const
{
    AV_SCOPE("BlobImports::invariants_hold");
    const ContextResources resources(ring_resource_);
    for (uint32_t slot = 0u; device_memory_ != nullptr && slot < max_imports; slot++) {
        if (device_memory_[slot] == 0u) continue;
        if (resources.chunk_slot(resources_[slot]) == ContextResources::chunk_slots) return false;
        if (blob_ids_[slot] == 0u || blob_ids_[slot] >= next_blob_id_) return false;
        for (uint32_t other = slot + 1u; other < max_imports; other++)
            if (device_memory_[other] == device_memory_[slot]) return false;
    }
    return true;
}

}  // namespace av
