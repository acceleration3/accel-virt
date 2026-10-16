#include "device/memory/host_blob_table.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/context/context_plane.hpp"
#include "device/host/host.hpp"

namespace av {

HostBlobTable::HostBlobTable(Host &host)
    : host_(host), contexts_(nullptr), space_(host), lock_(nullptr), blobs_(host), ids_(host, Pool::paged),
      placed_(host, Pool::paged)
{
    AV_SCOPE("HostBlobTable::HostBlobTable");
}

HostBlobTable::~HostBlobTable()
{
    AV_SCOPE("HostBlobTable::~HostBlobTable");
    for (uint32_t slot = ids_.next_live(0u); slot != ids_.no_slot; slot = ids_.next_live(slot + 1u))
        drop(ids_.key_at(slot));
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

HostBlob *HostBlobTable::blob_of(uint32_t id)
{
    AV_SCOPE("HostBlobTable::blob_of");
    HostBlob *const *const found = ids_.find(id);
    return found != nullptr ? *found : nullptr;
}

const HostBlob *HostBlobTable::blob_of(uint32_t id) const
{
    AV_SCOPE("HostBlobTable::blob_of");
    HostBlob *const *const found = ids_.find(id);
    return found != nullptr ? *found : nullptr;
}

uint32_t HostBlobTable::placed_from(uint64_t offset) const
{
    AV_SCOPE("HostBlobTable::placed_from");
    uint32_t low = 0u;
    uint32_t high = placed_.size();
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (placed_.at(middle).offset_ < offset) low = middle + 1u;
        else high = middle;
    }
    return low;
}

/* Placements are apart from each other, so only the one before `offset` and
 * the one at or after it can reach into the range. */
bool HostBlobTable::overlaps_placed(uint64_t offset, uint64_t size) const
{
    AV_SCOPE("HostBlobTable::overlaps_placed");
    const uint32_t next = placed_from(offset);
    if (next < placed_.size() && placed_.at(next).offset_ - offset < size) return true;
    return next != 0u && placed_.at(next - 1u).size_ > offset - placed_.at(next - 1u).offset_;
}

void HostBlobTable::unplace(uint64_t offset)
{
    AV_SCOPE("HostBlobTable::unplace");
    const uint32_t at = placed_from(offset);
    if (at < placed_.size() && placed_.at(at).offset_ == offset) placed_.remove_at(at);
}

void HostBlobTable::drop(uint32_t id)
{
    AV_SCOPE("HostBlobTable::drop");
    host_.lock_mutex(lock_);
    HostBlob *blob = nullptr;
    if (ids_.take(id, blob)) {
        if (blob->mapped()) unplace(blob->offset());
        static_cast<void>(blobs_.destroy(blob));
    }
    host_.unlock_mutex(lock_);
}

void HostBlobTable::destroy(uint32_t id)
{
    AV_SCOPE("HostBlobTable::destroy");
    drop(id);
    contexts_->free_memory(id);
}

/* The library may answer more than was asked - a driver rounds an allocation
 * up - but the blob is exactly what the guest placed a window range for, or
 * its mapping would run into the next range. Room to keep the blob is made
 * before the library is asked, so memory it allocated is never memory there
 * is nowhere to note. */
Status HostBlobTable::create(uint32_t id, uint32_t context, HostMemoryRequest request, uint64_t size)
{
    AV_SCOPE("HostBlobTable::create");
    host_.require_main_loop("HostBlobTable::create");
    if (lock_ == nullptr) return Status::invalid_state;
    if (!HostBlobId(id).valid()) return Status::not_found;
    if (blob_of(id) != nullptr) return Status::conflict;
    if (!request.valid() || size == 0u || size % HostmemSpace::page_size != 0u) return Status::invalid_parameter;
    host_.lock_mutex(lock_);
    const Status room = ids_.reserve(ids_.count() + 1u);
    host_.unlock_mutex(lock_);
    if (room != Status::ok) return Status::no_memory;
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
    HostBlob *blob = nullptr;
    if (blobs_.create(blob, host_, context, memory) != Status::ok) {
        host_.close_descriptor(memory.fd);
        contexts_->free_memory(id);
        return Status::no_memory;
    }
    uint32_t slot = 0u;
    static_cast<void>(ids_.insert(id, blob, slot));
    return Status::ok;
}

/* Room to note the placement first: a blob the host placed must never be one
 * the next placement could land on. */
Status HostBlobTable::map(uint32_t id, uint64_t offset, uint32_t &caching)
{
    AV_SCOPE("HostBlobTable::map");
    host_.require_main_loop("HostBlobTable::map");
    HostBlob *const blob = blob_of(id);
    if (blob == nullptr) return Status::not_found;
    const uint64_t size = blob->size();
    if (blob->mapped() || !space_.admits(offset, size) || overlaps_placed(offset, size))
        return Status::invalid_parameter;
    if (placed_.reserve(placed_.size() + 1u) != Status::ok) return Status::no_memory;
    const Status mapped = blob->map(offset);
    if (mapped != Status::ok) return mapped;
    static_cast<void>(placed_.insert_at(placed_from(offset), Placed{offset, size}));
    caching = blob->caching();
    return Status::ok;
}

Status HostBlobTable::unmap(uint32_t id)
{
    AV_SCOPE("HostBlobTable::unmap");
    host_.require_main_loop("HostBlobTable::unmap");
    HostBlob *const blob = blob_of(id);
    if (blob == nullptr) return Status::not_found;
    const uint64_t offset = blob->offset();
    if (blob->unmap() != Status::ok) return Status::invalid_parameter;
    unplace(offset);
    return Status::ok;
}

Status HostBlobTable::release(uint32_t id)
{
    AV_SCOPE("HostBlobTable::release");
    host_.require_main_loop("HostBlobTable::release");
    if (blob_of(id) == nullptr) return Status::not_found;
    destroy(id);
    return Status::ok;
}

void HostBlobTable::release_all()
{
    AV_SCOPE("HostBlobTable::release_all");
    host_.require_main_loop("HostBlobTable::release_all");
    for (uint32_t slot = ids_.next_live(0u); slot != ids_.no_slot; slot = ids_.next_live(slot + 1u))
        destroy(ids_.key_at(slot));
}

bool HostBlobTable::contains(uint32_t id) const
{
    AV_SCOPE("HostBlobTable::contains");
    return blob_of(id) != nullptr;
}

/* The lookup takes the lock: a creation or a release on the main loop writes
 * the table a worker would otherwise be reading. */
bool HostBlobTable::owns(uint32_t context, uint32_t id)
{
    AV_SCOPE("HostBlobTable::owns");
    if (lock_ == nullptr) return false;
    host_.lock_mutex(lock_);
    ScopeExit             unlock([this] { host_.unlock_mutex(lock_); });
    const HostBlob *const blob = blob_of(id);
    return blob != nullptr && blob->context() == context;
}

uint32_t HostBlobTable::live() const
{
    AV_SCOPE("HostBlobTable::live");
    return ids_.count();
}

void HostBlobTable::report(StateWriter &writer) const
{
    AV_SCOPE("HostBlobTable::report");
    writer.open("host_blobs", 0u);
    writer.number("live", live());
    ids_.for_each([&writer](uint32_t id, const HostBlob *blob) {
        writer.open("host_blob", id);
        writer.number("context", blob->context());
        writer.number("size", blob->size());
        writer.number("caching", blob->caching());
        writer.flag("mapped", blob->mapped());
        writer.number("offset", blob->offset());
        writer.close();
    });
    writer.close();
}

/* Placements are sorted and apart, there is one for each placed blob and no
 * other, and a blob lives only once there is a lock to hold it apart from a
 * worker's hold. */
bool HostBlobTable::invariants_hold() const
{
    AV_SCOPE("HostBlobTable::invariants_hold");
    if (lock_ == nullptr && live() != 0u) return false;
    if (!ids_.invariants_hold() || !blobs_.invariants_hold() || blobs_.live() != ids_.count()) return false;
    for (uint32_t index = 1u; index < placed_.size(); index++) {
        const Placed &before = placed_.at(index - 1u);
        if (placed_.at(index).offset_ < before.offset_ + before.size_) return false;
    }
    uint32_t mapped = 0u;
    bool     found = true;
    ids_.for_each([this, &mapped, &found](uint32_t, const HostBlob *blob) {
        if (!blob->mapped()) return;
        mapped++;
        const uint32_t at = placed_from(blob->offset());
        found = found && at < placed_.size() && placed_.at(at).offset_ == blob->offset() &&
                placed_.at(at).size_ == blob->size();
    });
    return found && mapped == placed_.size();
}

}  // namespace av
