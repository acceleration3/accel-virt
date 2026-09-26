#include "device/memory/blob_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "core/support/scope_exit.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/held_blob.hpp"

namespace av {

BlobTable::BlobTable(Host &host) : host_(host), space_(host), lock_(nullptr), slots_{}, retiring_{}
{
    AV_SCOPE("BlobTable::BlobTable");
}

BlobTable::~BlobTable()
{
    AV_SCOPE("BlobTable::~BlobTable");
    release_all();
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status BlobTable::initialize()
{
    AV_SCOPE("BlobTable::initialize");
    host_.require_main_loop("BlobTable::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_) == Status::ok ? Status::ok : Status::no_memory;
}

ObjectStorage<BlobResource> *BlobTable::slot_of(uint32_t id)
{
    AV_SCOPE("BlobTable::slot_of");
    for (ObjectStorage<BlobResource> &slot : slots_)
        if (slot.constructed() && slot.object().id() == id) return &slot;
    return nullptr;
}

Status BlobTable::create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset, uint32_t entries)
{
    AV_SCOPE("BlobTable::create_guest");
    host_.require_main_loop("BlobTable::create_guest");
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    sweep_retired();
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
    AV_SCOPE("BlobTable::create_host");
    host_.require_main_loop("BlobTable::create_host");
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    sweep_retired();
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
    AV_SCOPE("BlobTable::overlaps_mapping");
    for (const ObjectStorage<BlobResource> &slot : slots_)
        if (slot.constructed() && slot.object().overlaps(offset, size)) return true;
    return false;
}

Status BlobTable::map(uint32_t id, uint64_t offset)
{
    AV_SCOPE("BlobTable::map");
    host_.require_main_loop("BlobTable::map");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    const uint64_t size = slot->object().size();
    if (!space_.admits(offset, size) || overlaps_mapping(offset, size)) return Status::invalid_parameter;
    return slot->object().map(offset);
}

Status BlobTable::unmap(uint32_t id)
{
    AV_SCOPE("BlobTable::unmap");
    host_.require_main_loop("BlobTable::unmap");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    return slot != nullptr ? slot->object().unmap() : Status::not_found;
}

Status BlobTable::pin(uint32_t id, uint64_t minimum_size)
{
    AV_SCOPE("BlobTable::pin");
    host_.require_main_loop("BlobTable::pin");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (slot->object().size() < minimum_size) return Status::invalid_parameter;
    if (retiring(*slot)) return Status::invalid_state;
    slot->object().pin();
    return Status::ok;
}

Status BlobTable::backing(uint32_t id, BlobWindow &pages, void *&host_memory)
{
    AV_SCOPE("BlobTable::backing");
    host_.require_main_loop("BlobTable::backing");
    host_memory = nullptr;
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    pages = slot->object().window();
    host_memory = slot->object().host_memory();
    return Status::ok;
}

Status BlobTable::window(uint32_t id, BlobWindow &window)
{
    AV_SCOPE("BlobTable::window");
    host_.require_main_loop("BlobTable::window");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (!slot->object().pinned()) return Status::invalid_state;
    window = slot->object().window();
    return Status::ok;
}

void BlobTable::unpin(uint32_t id)
{
    AV_SCOPE("BlobTable::unpin");
    host_.require_main_loop("BlobTable::unpin");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return;
    slot->object().unpin();
    if (!retiring(*slot)) return;
    host_.lock_mutex(lock_);
    sweep_retired();
    host_.unlock_mutex(lock_);
}

Status BlobTable::release(uint32_t id)
{
    AV_SCOPE("BlobTable::release");
    host_.require_main_loop("BlobTable::release");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    host_.lock_mutex(lock_);
    ScopeExit    unlock([this] { host_.unlock_mutex(lock_); });
    const bool   held = slot->object().pinned() || slot->object().held();
    if (held) return Status::invalid_state;
    slot->destroy();
    retiring_[slot - slots_] = false;
    return Status::ok;
}

Status BlobTable::retire(uint32_t id)
{
    AV_SCOPE("BlobTable::retire");
    host_.require_main_loop("BlobTable::retire");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    host_.lock_mutex(lock_);
    retiring_[slot - slots_] = true;
    sweep_retired();
    host_.unlock_mutex(lock_);
    return Status::ok;
}

bool BlobTable::retiring(const ObjectStorage<BlobResource> &slot) const
{
    AV_SCOPE("BlobTable::retiring");
    return retiring_[&slot - slots_];
}

void BlobTable::sweep_retired()
{
    AV_SCOPE("BlobTable::sweep_retired");
    for (uint32_t index = 0u; index < max_blobs; index++) {
        if (!retiring_[index]) continue;
        const BlobResource &blob = slots_[index].object();
        if (blob.pinned() || blob.held()) continue;
        slots_[index].destroy();
        retiring_[index] = false;
    }
}

void BlobTable::release_all()
{
    AV_SCOPE("BlobTable::release_all");
    host_.require_main_loop("BlobTable::release_all");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    for (ObjectStorage<BlobResource> &slot : slots_) slot.destroy();
    for (bool &retiring : retiring_) retiring = false;
    host_.unlock_mutex(lock_);
}

bool BlobTable::contains(uint32_t id)
{
    AV_SCOPE("BlobTable::contains");
    host_.require_main_loop("BlobTable::contains");
    return slot_of(id) != nullptr;
}

Status BlobTable::size(uint32_t id, uint64_t &size)
{
    AV_SCOPE("BlobTable::size");
    host_.require_main_loop("BlobTable::size");
    const ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    size = slot->object().size();
    return Status::ok;
}

/* The lookup takes the lock too: a creation on the main loop writes the slot a
 * worker would otherwise be reading. */
Status BlobTable::hold(uint32_t id, HeldBlob &held)
{
    AV_SCOPE("BlobTable::hold");
    held = HeldBlob();
    if (lock_ == nullptr) return Status::not_found;
    host_.lock_mutex(lock_);
    ScopeExit                          unlock([this] { host_.unlock_mutex(lock_); });
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    return retiring(*slot) ? Status::invalid_state : slot->object().hold(held);
}

void BlobTable::release_hold(uint32_t id)
{
    AV_SCOPE("BlobTable::release_hold");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot != nullptr) slot->object().release_hold();
    host_.unlock_mutex(lock_);
}

uint32_t BlobTable::live() const
{
    AV_SCOPE("BlobTable::live");
    uint32_t count = 0u;
    for (const ObjectStorage<BlobResource> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

/* Ids are unique, no two mappings share a byte of the region, no blob lives
 * that a worker's hold could not be held apart from, and only a live blob is
 * retiring: a sweep reads the blob a retiring mark names. */
bool BlobTable::invariants_hold() const
{
    AV_SCOPE("BlobTable::invariants_hold");
    if (lock_ == nullptr && live() != 0u) return false;
    for (uint32_t first = 0u; first < max_blobs; first++) {
        if (retiring_[first] && !slots_[first].constructed()) return false;
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
