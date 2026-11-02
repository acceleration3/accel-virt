#include "miniport/memory/host_window.hpp"

#include "core/platform/mutex_guard.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

HostWindow::HostWindow(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::host_window), map_(nullptr), base_(0u), pages_(0u), used_(0u),
      offered_(false)
{
    AV_SCOPE("HostWindow::HostWindow");
}

HostWindow::~HostWindow()
{
    AV_SCOPE("HostWindow::~HostWindow");
    if (map_ != nullptr) platform_.release(map_);
}

Status HostWindow::initialize(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("HostWindow::initialize");
    platform_.require_irql_at_most(Irql::passive, "HostWindow::initialize");
    MutexGuard             locked(lock_);
    InvariantScope<HostWindow> guard(platform_, *this, "initialize");
    const uint64_t         pages = bytes / page_bytes;
    if (pages == 0u || pages > UINT32_MAX) return Status::invalid_parameter;
    if (map_ != nullptr) {
        if (pages != pages_ || (base != base_ && used_ != 0u)) return Status::invalid_state;
        base_ = base;
        offered_ = true;
        return Status::ok;
    }
    const size_t words = static_cast<size_t>((pages + bits_per_word - 1u) / bits_per_word);
    map_ = static_cast<uint64_t *>(platform_.allocate(words * sizeof(uint64_t), Pool::non_paged));
    if (map_ == nullptr) return Status::no_memory;
    platform_.zero(map_, words * sizeof(uint64_t));
    base_ = base;
    pages_ = static_cast<uint32_t>(pages);
    offered_ = true;
    return Status::ok;
}

void HostWindow::withdraw()
{
    AV_SCOPE("HostWindow::withdraw");
    platform_.require_irql_at_most(Irql::passive, "HostWindow::withdraw");
    MutexGuard locked(lock_);
    offered_ = false;
}

bool HostWindow::offered() const
{
    AV_SCOPE("HostWindow::offered");
    return offered_;
}

Status HostWindow::take(uint32_t pages, uint32_t &first)
{
    AV_SCOPE("HostWindow::take");
    platform_.require_irql_at_most(Irql::passive, "HostWindow::take");
    MutexGuard                 locked(lock_);
    InvariantScope<HostWindow> guard(platform_, *this, "take");
    first = 0u;
    if (!offered_) return Status::not_supported;
    if (pages == 0u) return Status::invalid_parameter;
    uint32_t run = 0u;
    for (uint64_t page = 0u; page < pages_; page++) {
        if (page % bits_per_word == 0u && map_[page / bits_per_word] == UINT64_MAX) {
            page += bits_per_word - 1u;
            run = 0u;
            continue;
        }
        const bool used = (map_[page / bits_per_word] >> (page % bits_per_word) & 1u) != 0u;
        run = used ? 0u : run + 1u;
        if (run < pages) continue;
        first = static_cast<uint32_t>(page + 1u - pages);
        mark(first, pages);
        return Status::ok;
    }
    return Status::no_space;
}

void HostWindow::give_back(uint32_t first, uint32_t pages)
{
    AV_SCOPE("HostWindow::give_back");
    platform_.require_irql_at_most(Irql::passive, "HostWindow::give_back");
    MutexGuard                 locked(lock_);
    InvariantScope<HostWindow> guard(platform_, *this, "give_back");
    if (first >= pages_) return;
    const uint32_t last = pages > pages_ - first ? pages_ : first + pages;
    for (uint32_t page = first; page < last; page++) {
        uint64_t &word = map_[page / bits_per_word];
        const uint64_t bit = uint64_t{1} << (page % bits_per_word);
        if ((word & bit) == 0u) continue;
        word &= ~bit;
        used_--;
    }
}

uint64_t HostWindow::physical(uint32_t first) const
{
    AV_SCOPE("HostWindow::physical");
    return base_ + uint64_t{first} * page_bytes;
}

uint32_t HostWindow::pages_in_use() const
{
    AV_SCOPE("HostWindow::pages_in_use");
    return used_;
}

void HostWindow::mark(uint32_t first, uint32_t pages)
{
    AV_SCOPE("HostWindow::mark");
    for (uint32_t page = first; page < first + pages; page++)
        map_[page / bits_per_word] |= uint64_t{1} << (page % bits_per_word);
    used_ += pages;
}

bool HostWindow::invariants_hold() const
{
    AV_SCOPE("HostWindow::invariants_hold");
    if (map_ == nullptr) return used_ == 0u && pages_ == 0u;
    uint32_t set = 0u;
    for (uint64_t word = 0u; word < (uint64_t{pages_} + bits_per_word - 1u) / bits_per_word; word++)
        set += static_cast<uint32_t>(__builtin_popcountll(map_[word]));
    return set == used_ && used_ <= pages_;
}

}
