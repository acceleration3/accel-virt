#include "executor/sharing/share_index.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ShareIndex::DomainIds::DomainIds(BlockSource &blocks) : ids_(blocks, Pool::paged)
{
    AV_SCOPE("ShareIndex::DomainIds::DomainIds");
}

KeyedSlots<uint64_t, Share *> &ShareIndex::DomainIds::ids()
{
    AV_SCOPE("ShareIndex::DomainIds::ids");
    return ids_;
}

ShareIndex::ShareIndex(BlockSource &blocks)
    : blocks_(blocks), tables_(blocks), domains_(blocks, Pool::paged), count_(0u)
{
    AV_SCOPE("ShareIndex::ShareIndex");
}

Share *ShareIndex::find(uint32_t domain, uint64_t id) const
{
    AV_SCOPE("ShareIndex::find");
    DomainIds *const *const table = domains_.find(domain);
    if (table == nullptr) return nullptr;
    Share *const *const share = (*table)->ids().find(id);
    return share != nullptr ? *share : nullptr;
}

Status ShareIndex::insert(uint32_t domain, uint64_t id, Share *share)
{
    AV_SCOPE("ShareIndex::insert");
    if (id == 0u || share == nullptr) return Status::invalid_parameter;
    DomainIds *const *const held = domains_.find(domain);
    DomainIds              *table = held != nullptr ? *held : nullptr;
    if (table == nullptr) {
        const Status room = domains_.reserve(domains_.count() + 1u);
        if (room != Status::ok) return room;
        const Status made = tables_.create(table, blocks_);
        if (made != Status::ok) return made;
        uint32_t slot = 0u;
        /* Room was made above. */
        static_cast<void>(domains_.insert(domain, table, slot));
    }
    uint32_t     slot = 0u;
    const Status inserted = table->ids().insert(id, share, slot);
    if (inserted == Status::ok) count_++;
    if (table->ids().count() == 0u) {
        domains_.erase(domain);
        static_cast<void>(tables_.destroy(table));
    }
    return inserted;
}

void ShareIndex::remove(uint32_t domain, uint64_t id)
{
    AV_SCOPE("ShareIndex::remove");
    DomainIds *const *const held = domains_.find(domain);
    if (held == nullptr) return;
    DomainIds *const table = *held;
    if (!table->ids().erase(id)) return;
    count_--;
    if (table->ids().count() != 0u) return;
    domains_.erase(domain);
    static_cast<void>(tables_.destroy(table));
}

uint32_t ShareIndex::count() const
{
    AV_SCOPE("ShareIndex::count");
    return count_;
}

}  // namespace av
