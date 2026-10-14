#include "device/memory/host_blob_table.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/context/context_plane.hpp"
#include "device/host/host.hpp"

namespace av {

HostBlobTable::HostBlobTable(Host &host) : host_(host), contexts_(nullptr), space_(host), lock_(nullptr), slots_()
{
    AV_SCOPE("HostBlobTable::HostBlobTable");
}

HostBlobTable::~HostBlobTable()
{
    AV_SCOPE("HostBlobTable::~HostBlobTable");
    for (uint32_t slot = 0u; slot < HostBlobId::count; slot++)
        if (slots_[slot].constructed()) drop(slot);
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status HostBlobTable::initialize(ContextPlane &contexts)
{
    AV_SCOPE("HostBlobTable::initialize");
    host_.require_main_loop("HostBlobTable::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    if (host_.create_mutex(lock_) != Status::ok) return Status::no_memory;
    contexts_ = &contexts;
    return Status::ok;
}

ObjectStorage<HostBlob> *HostBlobTable::slot_of(uint32_t id)
{
    AV_SCOPE("HostBlobTable::slot_of");
    const HostBlobId named(id);
    if (!named.valid() || !slots_[named.slot()].constructed()) return nullptr;
    return &slots_[named.slot()];
}

const ObjectStorage<HostBlob> *HostBlobTable::slot_of(uint32_t id) const
{
    AV_SCOPE("HostBlobTable::slot_of");
    const HostBlobId named(id);
    if (!named.valid() || !slots_[named.slot()].constructed()) return nullptr;
    return &slots_[named.slot()];
}

void HostBlobTable::drop(uint32_t slot)
{
    AV_SCOPE("HostBlobTable::drop");
    host_.lock_mutex(lock_);
    slots_[slot].destroy();
    host_.unlock_mutex(lock_);
}

void HostBlobTable::destroy(uint32_t slot)
{
    AV_SCOPE("HostBlobTable::destroy");
    drop(slot);
    contexts_->free_memory(HostBlobId::first + slot);
}

/* The library may answer more than was asked - a driver rounds an allocation
 * up - but the blob is exactly what the guest placed a window range for, or
 * its mapping would run into the next range. */
Status HostBlobTable::create(uint32_t id, uint32_t context, HostMemoryRequest request, uint64_t size)
{
    AV_SCOPE("HostBlobTable::create");
    host_.require_main_loop("HostBlobTable::create");
    if (lock_ == nullptr) return Status::invalid_state;
    const HostBlobId named(id);
    if (!named.valid()) return Status::not_found;
    if (slots_[named.slot()].constructed()) return Status::conflict;
    if (!request.valid() || size == 0u || size % HostmemSpace::page_size != 0u) return Status::invalid_parameter;
    AvHostMemory memory = {-1, 0u, 0u};
    if (contexts_->allocate_memory(context, id, size, request.encoded(), memory) != Status::ok)
        return Status::no_memory;
    const bool cached = memory.caching == AV_MEMORY_WRITE_BACK || memory.caching == AV_MEMORY_WRITE_COMBINED;
    if (memory.fd < 0 || memory.size < size || !cached) {
        if (memory.fd >= 0) host_.close_descriptor(memory.fd);
        contexts_->free_memory(id);
        return Status::no_memory;
    }
    memory.size = size;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    static_cast<void>(slots_[named.slot()].construct(host_, context, memory));
    return Status::ok;
}

bool HostBlobTable::overlaps_mapping(uint64_t offset, uint64_t size) const
{
    AV_SCOPE("HostBlobTable::overlaps_mapping");
    for (const ObjectStorage<HostBlob> &slot : slots_)
        if (slot.constructed() && slot.object().overlaps(offset, size)) return true;
    return false;
}

Status HostBlobTable::map(uint32_t id, uint64_t offset, uint32_t &caching)
{
    AV_SCOPE("HostBlobTable::map");
    host_.require_main_loop("HostBlobTable::map");
    ObjectStorage<HostBlob> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    HostBlob      &blob = slot->object();
    const uint64_t size = blob.size();
    if (blob.mapped() || !space_.admits(offset, size) || overlaps_mapping(offset, size))
        return Status::invalid_parameter;
    const Status mapped = blob.map(offset);
    if (mapped == Status::ok) caching = blob.caching();
    return mapped;
}

Status HostBlobTable::unmap(uint32_t id)
{
    AV_SCOPE("HostBlobTable::unmap");
    host_.require_main_loop("HostBlobTable::unmap");
    ObjectStorage<HostBlob> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    return slot->object().unmap() == Status::ok ? Status::ok : Status::invalid_parameter;
}

Status HostBlobTable::release(uint32_t id)
{
    AV_SCOPE("HostBlobTable::release");
    host_.require_main_loop("HostBlobTable::release");
    if (slot_of(id) == nullptr) return Status::not_found;
    destroy(HostBlobId(id).slot());
    return Status::ok;
}

void HostBlobTable::release_all()
{
    AV_SCOPE("HostBlobTable::release_all");
    host_.require_main_loop("HostBlobTable::release_all");
    for (uint32_t slot = 0u; slot < HostBlobId::count; slot++)
        if (slots_[slot].constructed()) destroy(slot);
}

bool HostBlobTable::contains(uint32_t id) const
{
    AV_SCOPE("HostBlobTable::contains");
    return slot_of(id) != nullptr;
}

/* The lookup takes the lock: a creation or a release on the main loop writes
 * the slot a worker would otherwise be reading. */
bool HostBlobTable::owns(uint32_t context, uint32_t id)
{
    AV_SCOPE("HostBlobTable::owns");
    if (lock_ == nullptr) return false;
    host_.lock_mutex(lock_);
    ScopeExit                            unlock([this] { host_.unlock_mutex(lock_); });
    const ObjectStorage<HostBlob> *const slot = slot_of(id);
    return slot != nullptr && slot->object().context() == context;
}

uint32_t HostBlobTable::live() const
{
    AV_SCOPE("HostBlobTable::live");
    uint32_t count = 0u;
    for (const ObjectStorage<HostBlob> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

void HostBlobTable::report(StateWriter &writer) const
{
    AV_SCOPE("HostBlobTable::report");
    writer.open("host_blobs", 0u);
    writer.number("live", live());
    for (uint32_t slot = 0u; slot < HostBlobId::count; slot++) {
        if (!slots_[slot].constructed()) continue;
        const HostBlob &blob = slots_[slot].object();
        writer.open("host_blob", HostBlobId::first + slot);
        writer.number("context", blob.context());
        writer.number("size", blob.size());
        writer.number("caching", blob.caching());
        writer.flag("mapped", blob.mapped());
        writer.number("offset", blob.offset());
        writer.close();
    }
    writer.close();
}

/* No two mappings share a byte of the region, and a blob lives only once
 * there is a lock to hold it apart from a worker's hold. */
bool HostBlobTable::invariants_hold() const
{
    AV_SCOPE("HostBlobTable::invariants_hold");
    if (lock_ == nullptr && live() != 0u) return false;
    for (uint32_t first = 0u; first < HostBlobId::count; first++) {
        if (!slots_[first].constructed() || !slots_[first].object().mapped()) continue;
        const HostBlob &blob = slots_[first].object();
        for (uint32_t second = first + 1u; second < HostBlobId::count; second++)
            if (slots_[second].constructed() && slots_[second].object().overlaps(blob.offset(), blob.size()))
                return false;
    }
    return true;
}

}  // namespace av
