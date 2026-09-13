#include "core/memory/hostmem_placement.hpp"

#include "core/platform/mutex_guard.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

HostmemPlacement::HostmemPlacement(Platform &platform)
    : platform_(platform), mutex_(platform, LockRank::hostmem_placement), offsets_{}, pages_{}, owners_{},
      count_(0u), region_pages_(0u) {}

void HostmemPlacement::open(uint64_t region_bytes)
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::open");
    InvariantScope<HostmemPlacement> guard(platform_, *this, "open");
    MutexGuard held(mutex_);
    count_ = 0u;
    region_pages_ = region_bytes / page_bytes;
}

void HostmemPlacement::close()
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::close");
    InvariantScope<HostmemPlacement> guard(platform_, *this, "close");
    MutexGuard held(mutex_);
    count_ = 0u;
    region_pages_ = 0u;
}

Status HostmemPlacement::take(uint64_t offset, uint64_t bytes, const void *owner)
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::take");
    InvariantScope<HostmemPlacement> guard(platform_, *this, "take");
    if (owner == nullptr || bytes == 0u || (offset & (page_bytes - 1u)) != 0u)
        return Status::invalid_parameter;
    MutexGuard     held(mutex_);
    const uint64_t page = offset / page_bytes;
    const uint64_t pages = (bytes + page_bytes - 1u) / page_bytes;
    if (pages > region_pages_ || page > region_pages_ - pages) return Status::invalid_parameter;
    if (index_of(owner) != max_blobs) return Status::invalid_state;
    if (count_ == max_blobs) return Status::no_space;
    bool           clear = false;
    const uint32_t at = place_for(page, pages, clear);
    if (!clear) return Status::conflict;
    insert(at, page, pages, owner);
    return Status::ok;
}

/* Where a range of `pages` from `page` belongs in offset order, and whether it
 * overlaps the placements on either side of it. */
uint32_t HostmemPlacement::place_for(uint64_t page, uint64_t pages, bool &clear) const
{
    uint32_t at = 0u;
    while (at < count_ && offsets_[at] < page) at++;
    const bool after_previous = at == 0u || offsets_[at - 1u] + pages_[at - 1u] <= page;
    const bool before_next = at == count_ || page + pages <= offsets_[at];
    clear = after_previous && before_next;
    return at;
}

uint32_t HostmemPlacement::index_of(const void *owner) const
{
    for (uint32_t index = 0u; index < count_; index++)
        if (owners_[index] == owner) return index;
    return max_blobs;
}

void HostmemPlacement::insert(uint32_t at, uint64_t offset, uint64_t pages, const void *owner)
{
    for (uint32_t index = count_; index > at; index--) {
        offsets_[index] = offsets_[index - 1u];
        pages_[index] = pages_[index - 1u];
        owners_[index] = owners_[index - 1u];
    }
    offsets_[at] = offset;
    pages_[at] = pages;
    owners_[at] = owner;
    count_++;
}

void HostmemPlacement::release(const void *owner)
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::release");
    InvariantScope<HostmemPlacement> guard(platform_, *this, "release");
    if (owner == nullptr) return;
    MutexGuard     held(mutex_);
    const uint32_t at = index_of(owner);
    if (at == max_blobs) return;
    for (uint32_t index = at + 1u; index < count_; index++) {
        offsets_[index - 1u] = offsets_[index];
        pages_[index - 1u] = pages_[index];
        owners_[index - 1u] = owners_[index];
    }
    count_--;
}

const void *HostmemPlacement::holder(uint64_t offset) const
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::holder");
    const uint64_t page = offset / page_bytes;
    for (uint32_t index = 0u; index < count_; index++)
        if (page >= offsets_[index] && page - offsets_[index] < pages_[index]) return owners_[index];
    return nullptr;
}

uint32_t HostmemPlacement::live() const
{
    platform_.require_irql_at_most(Irql::passive, "HostmemPlacement::live");
    return count_;
}

bool HostmemPlacement::invariants_hold() const
{
    if (count_ > max_blobs) return false;
    uint64_t reach = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        if (owners_[index] == nullptr || pages_[index] == 0u) return false;
        if (offsets_[index] < reach) return false;                 /* in order, and no overlap */
        reach = offsets_[index] + pages_[index];
        if (reach > region_pages_) return false;                   /* wholly inside the region */
    }
    return true;
}

}  // namespace av
