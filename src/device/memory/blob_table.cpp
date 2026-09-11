#include "device/memory/blob_table.hpp"

#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobTable::BlobTable(Host &host) : host_(host), space_(host), slots_{} {}

BlobTable::~BlobTable() { release_all(); }

ObjectStorage<BlobResource> *BlobTable::slot_of(uint32_t id)
{
    for (ObjectStorage<BlobResource> &slot : slots_)
        if (slot.constructed() && slot.object().id() == id) return &slot;
    return nullptr;
}

Status BlobTable::create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset, uint32_t entries)
{
    host_.require_main_loop("BlobTable::create_guest");
    if (slot_of(id) != nullptr) return Status::conflict;
    for (ObjectStorage<BlobResource> &slot : slots_) {
        if (slot.construct(host_, id) != Status::ok) continue;   /* occupied */
        const Status status = slot.object().back_with_guest(request, offset, entries, size);
        if (status != Status::ok) slot.destroy();
        return status;
    }
    return Status::no_space;
}

Status BlobTable::create_host(uint32_t id, uint64_t size)
{
    host_.require_main_loop("BlobTable::create_host");
    if (slot_of(id) != nullptr) return Status::conflict;
    for (ObjectStorage<BlobResource> &slot : slots_) {
        if (slot.construct(host_, id) != Status::ok) continue;   /* occupied */
        const Status status = slot.object().back_with_host(size);
        if (status != Status::ok) slot.destroy();
        return status;
    }
    return Status::no_space;
}

bool BlobTable::overlaps_mapping(uint64_t offset, uint64_t size) const
{
    for (const ObjectStorage<BlobResource> &slot : slots_)
        if (slot.constructed() && slot.object().overlaps(offset, size)) return true;
    return false;
}

Status BlobTable::map(uint32_t id, uint64_t offset)
{
    host_.require_main_loop("BlobTable::map");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    const uint64_t size = slot->object().size();
    if (!space_.admits(offset, size) || overlaps_mapping(offset, size)) return Status::invalid_parameter;
    return slot->object().map(offset);
}

Status BlobTable::unmap(uint32_t id)
{
    host_.require_main_loop("BlobTable::unmap");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    return slot != nullptr ? slot->object().unmap() : Status::not_found;
}

Status BlobTable::pin(uint32_t id, uint64_t minimum_size)
{
    host_.require_main_loop("BlobTable::pin");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (slot->object().size() < minimum_size) return Status::invalid_parameter;
    slot->object().pin();
    return Status::ok;
}

Status BlobTable::window(uint32_t id, BlobWindow &window)
{
    host_.require_main_loop("BlobTable::window");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (!slot->object().pinned()) return Status::invalid_state;
    window = slot->object().window();
    return Status::ok;
}

void BlobTable::unpin(uint32_t id)
{
    host_.require_main_loop("BlobTable::unpin");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot != nullptr) slot->object().unpin();
}

Status BlobTable::release(uint32_t id)
{
    host_.require_main_loop("BlobTable::release");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (slot->object().pinned()) return Status::invalid_state;
    slot->destroy();
    return Status::ok;
}

void BlobTable::release_all()
{
    host_.require_main_loop("BlobTable::release_all");
    for (ObjectStorage<BlobResource> &slot : slots_) slot.destroy();
}

bool BlobTable::contains(uint32_t id)
{
    host_.require_main_loop("BlobTable::contains");
    return slot_of(id) != nullptr;
}

uint32_t BlobTable::live() const
{
    uint32_t count = 0u;
    for (const ObjectStorage<BlobResource> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

/* Ids are unique, and no two mappings share a byte of the region. */
bool BlobTable::invariants_hold() const
{
    for (uint32_t first = 0u; first < max_blobs; first++) {
        if (!slots_[first].constructed()) continue;
        const BlobResource &blob = slots_[first].object();
        for (uint32_t second = first + 1u; second < max_blobs; second++) {
            if (!slots_[second].constructed()) continue;
            const BlobResource &other = slots_[second].object();
            if (other.id() == blob.id() || (other.mapped() && blob.overlaps(other.offset(), other.size())))
                return false;
        }
    }
    return true;
}

}  // namespace av
