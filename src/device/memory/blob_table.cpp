#include "device/memory/blob_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "core/support/scope_exit.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobTable::BlobTable(Host &host)
    : host_(host), lock_(nullptr), blobs_(host), ids_(host, Pool::paged)
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

BlobTable::Entry *BlobTable::entry_of(uint32_t id)
{
    AV_SCOPE("BlobTable::entry_of");
    return ids_.find(id);
}

Status BlobTable::create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset, uint32_t entries)
{
    AV_SCOPE("BlobTable::create_guest");
    host_.require_main_loop("BlobTable::create_guest");
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    sweep_retired();
    if (entry_of(id) != nullptr) return Status::conflict;
    if (ids_.reserve(ids_.count() + 1u) != Status::ok) return Status::no_memory;
    BlobResource *blob = nullptr;
    if (blobs_.create(blob, host_, id) != Status::ok) return Status::no_memory;
    const Status backed = blob->back_with_guest(request, offset, entries, size);
    uint32_t     slot = 0u;
    /* Room was made above, so the insert cannot fail. */
    if (backed == Status::ok) return ids_.insert(id, Entry{blob, false}, slot);
    static_cast<void>(blobs_.destroy(blob));
    return backed;
}

Status BlobTable::pin(uint32_t id, uint64_t minimum_size)
{
    AV_SCOPE("BlobTable::pin");
    host_.require_main_loop("BlobTable::pin");
    Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    if (entry->blob_->size() < minimum_size) return Status::invalid_parameter;
    if (entry->retiring_) return Status::invalid_state;
    entry->blob_->pin();
    return Status::ok;
}

Status BlobTable::backing(uint32_t id, BlobWindow &pages)
{
    AV_SCOPE("BlobTable::backing");
    host_.require_main_loop("BlobTable::backing");
    const Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    pages = entry->blob_->window();
    return Status::ok;
}

Status BlobTable::window(uint32_t id, BlobWindow &window)
{
    AV_SCOPE("BlobTable::window");
    host_.require_main_loop("BlobTable::window");
    const Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    if (!entry->blob_->pinned()) return Status::invalid_state;
    window = entry->blob_->window();
    return Status::ok;
}

Status BlobTable::view(uint32_t id, const uint8_t *&base)
{
    AV_SCOPE("BlobTable::view");
    host_.require_main_loop("BlobTable::view");
    base = nullptr;
    const Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    if (!entry->blob_->pinned()) return Status::invalid_state;
    return entry->blob_->view(base);
}

void BlobTable::unpin(uint32_t id)
{
    AV_SCOPE("BlobTable::unpin");
    host_.require_main_loop("BlobTable::unpin");
    Entry *const entry = entry_of(id);
    if (entry == nullptr) return;
    entry->blob_->unpin();
    if (!entry->retiring_) return;
    host_.lock_mutex(lock_);
    sweep_retired();
    host_.unlock_mutex(lock_);
}

Status BlobTable::release(uint32_t id)
{
    AV_SCOPE("BlobTable::release");
    host_.require_main_loop("BlobTable::release");
    Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    host_.lock_mutex(lock_);
    ScopeExit    unlock([this] { host_.unlock_mutex(lock_); });
    if (entry->blob_->pinned()) return Status::invalid_state;
    static_cast<void>(blobs_.destroy(entry->blob_));
    ids_.erase(id);
    return Status::ok;
}

Status BlobTable::retire(uint32_t id)
{
    AV_SCOPE("BlobTable::retire");
    host_.require_main_loop("BlobTable::retire");
    Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    host_.lock_mutex(lock_);
    entry->retiring_ = true;
    sweep_retired();
    host_.unlock_mutex(lock_);
    return Status::ok;
}

void BlobTable::sweep_retired()
{
    AV_SCOPE("BlobTable::sweep_retired");
    ids_.erase_if([this](uint32_t, Entry &entry) {
        if (!entry.retiring_ || entry.blob_->pinned()) return false;
        static_cast<void>(blobs_.destroy(entry.blob_));
        return true;
    });
}

void BlobTable::release_all()
{
    AV_SCOPE("BlobTable::release_all");
    host_.require_main_loop("BlobTable::release_all");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    blobs_.destroy_all();
    ids_.clear();
    host_.unlock_mutex(lock_);
}

bool BlobTable::contains(uint32_t id)
{
    AV_SCOPE("BlobTable::contains");
    host_.require_main_loop("BlobTable::contains");
    return entry_of(id) != nullptr;
}

Status BlobTable::size(uint32_t id, uint64_t &size)
{
    AV_SCOPE("BlobTable::size");
    host_.require_main_loop("BlobTable::size");
    const Entry *const entry = entry_of(id);
    if (entry == nullptr) return Status::not_found;
    size = entry->blob_->size();
    return Status::ok;
}

uint32_t BlobTable::live() const
{
    AV_SCOPE("BlobTable::live");
    return ids_.count();
}

/* No blob lives that a worker's read could not be held apart from, and each
 * id names a live blob of that id and no other id the same one. */
bool BlobTable::invariants_hold() const
{
    AV_SCOPE("BlobTable::invariants_hold");
    if (lock_ == nullptr && live() != 0u) return false;
    if (!ids_.invariants_hold() || !blobs_.invariants_hold() || blobs_.live() != ids_.count()) return false;
    bool named = true;
    ids_.for_each([this, &named](uint32_t id, const Entry &entry) {
        named = named && blobs_.find(entry.blob_) == entry.blob_ && entry.blob_->id() == id;
    });
    return named;
}

}  // namespace av
