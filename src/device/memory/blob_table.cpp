#include "device/memory/blob_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "core/support/scope_exit.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobTable::BlobTable(Host &host) : host_(host), lock_(nullptr), slots_{}, retiring_{}
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

Status BlobTable::backing(uint32_t id, BlobWindow &pages)
{
    AV_SCOPE("BlobTable::backing");
    host_.require_main_loop("BlobTable::backing");
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    pages = slot->object().window();
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

Status BlobTable::view(uint32_t id, const uint8_t *&base)
{
    AV_SCOPE("BlobTable::view");
    host_.require_main_loop("BlobTable::view");
    base = nullptr;
    ObjectStorage<BlobResource> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (!slot->object().pinned()) return Status::invalid_state;
    return slot->object().view(base);
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
    if (slot->object().pinned()) return Status::invalid_state;
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
        if (blob.pinned()) continue;
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

uint32_t BlobTable::live() const
{
    AV_SCOPE("BlobTable::live");
    uint32_t count = 0u;
    for (const ObjectStorage<BlobResource> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

/* Ids are unique, no blob lives that a worker's read could not be held apart
 * from, and only a live blob is retiring: a sweep reads the blob a retiring
 * mark names. */
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
            if (other.id() == blob.id()) return false;
        }
    }
    return true;
}

}  // namespace av
