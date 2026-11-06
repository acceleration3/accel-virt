#include "device/memory/lent_pages.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

LentPages::LentPages(BlockSource &blocks) : pages_(blocks, Pool::paged)
{
    AV_SCOPE("LentPages::LentPages");
}

bool LentPages::lent(uint64_t address) const
{
    AV_SCOPE("LentPages::lent");
    return pages_.find(address / page_size) != nullptr;
}

GuestRamLending *LentPages::owner_of(uint64_t address) const
{
    AV_SCOPE("LentPages::owner_of");
    GuestRamLending *const *const found = pages_.find(address / page_size);
    return found != nullptr ? *found : nullptr;
}

Status LentPages::claim(uint64_t address, uint64_t bytes, GuestRamLending &owner)
{
    AV_SCOPE("LentPages::claim");
    const uint64_t first = address / page_size;
    const uint64_t pages = bytes / page_size;
    if (pages > pages_.most_entries - pages_.count()) return Status::no_memory;
    for (uint64_t page = first; page < first + pages; page++)
        if (pages_.find(page) != nullptr) return Status::conflict;
    if (pages_.reserve(pages_.count() + static_cast<uint32_t>(pages)) != Status::ok) return Status::no_memory;
    uint32_t slot = 0u;
    for (uint64_t page = first; page < first + pages; page++) static_cast<void>(pages_.insert(page, &owner, slot));
    return Status::ok;
}

void LentPages::release(uint64_t address, uint64_t bytes)
{
    AV_SCOPE("LentPages::release");
    const uint64_t first = address / page_size;
    for (uint64_t page = first; page < first + bytes / page_size; page++) static_cast<void>(pages_.erase(page));
}

uint64_t LentPages::count() const
{
    AV_SCOPE("LentPages::count");
    return pages_.count();
}

bool LentPages::invariants_hold() const
{
    AV_SCOPE("LentPages::invariants_hold");
    return pages_.invariants_hold();
}

}
