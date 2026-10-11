#include "executor/sharing/share_index.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* splitmix64's finaliser: ids are minted in order, so their low bits alone
 * would fill one run of the table. */
constexpr uint64_t mixed(uint32_t domain, uint64_t id)
{
    uint64_t value = id ^ (uint64_t{domain} << 48u);
    value ^= value >> 30u;
    value *= 0xBF58476D1CE4E5B9u;
    value ^= value >> 27u;
    value *= 0x94D049BB133111EBu;
    return value ^ (value >> 31u);
}

}  // namespace

ShareIndex::ShareIndex(BlockSource &blocks)
    : blocks_(blocks), entries_(nullptr), capacity_(0u), count_(0u), removed_(0u)
{
    AV_SCOPE("ShareIndex::ShareIndex");
}

ShareIndex::~ShareIndex()
{
    AV_SCOPE("ShareIndex::~ShareIndex");
    if (entries_ != nullptr) blocks_.release(entries_);
}

Share *ShareIndex::find(uint32_t domain, uint64_t id) const
{
    AV_SCOPE("ShareIndex::find");
    if (id == 0u || entries_ == nullptr) return nullptr;
    const Entry *const entry = live_entry(domain, id);
    return entry != nullptr ? entry->share_ : nullptr;
}

Status ShareIndex::insert(uint32_t domain, uint64_t id, Share *share)
{
    AV_SCOPE("ShareIndex::insert");
    if (id == 0u || share == nullptr) return Status::invalid_parameter;
    if (find(domain, id) != nullptr) return Status::conflict;
    /* Grown at half full, counting tombstones, so a probe always ends. */
    if (2u * (count_ + removed_ + 1u) > capacity_) {
        const Status grown = grow();
        if (grown != Status::ok) return grown;
    }
    Entry *const entry = free_entry(domain, id);
    if (entry->removed_) removed_--;
    *entry = Entry{id, share, domain, false};
    count_++;
    return Status::ok;
}

void ShareIndex::remove(uint32_t domain, uint64_t id)
{
    AV_SCOPE("ShareIndex::remove");
    if (id == 0u || entries_ == nullptr) return;
    Entry *const entry = live_entry(domain, id);
    if (entry == nullptr) return;
    entry->share_ = nullptr;
    entry->removed_ = true;
    count_--;
    removed_++;
}

uint32_t ShareIndex::count() const
{
    AV_SCOPE("ShareIndex::count");
    return count_;
}

uint32_t ShareIndex::start_of(uint32_t domain, uint64_t id) const
{
    AV_SCOPE("ShareIndex::start_of");
    return static_cast<uint32_t>(mixed(domain, id)) & (capacity_ - 1u);
}

ShareIndex::Entry *ShareIndex::live_entry(uint32_t domain, uint64_t id) const
{
    AV_SCOPE("ShareIndex::live_entry");
    for (uint32_t index = start_of(domain, id);; index = (index + 1u) & (capacity_ - 1u)) {
        Entry &entry = entries_[index];
        if (entry.removed_) continue;
        if (entry.id_ == 0u) return nullptr;
        if (entry.id_ == id && entry.domain_ == domain) return &entry;
    }
}

ShareIndex::Entry *ShareIndex::free_entry(uint32_t domain, uint64_t id) const
{
    AV_SCOPE("ShareIndex::free_entry");
    for (uint32_t index = start_of(domain, id);; index = (index + 1u) & (capacity_ - 1u)) {
        Entry &entry = entries_[index];
        if (entry.removed_ || entry.id_ == 0u) return &entry;
    }
}

Status ShareIndex::grow()
{
    AV_SCOPE("ShareIndex::grow");
    /* Mostly tombstones - ids minted and released over and over, as a window
     * presenting does - is a rebuild at the same size, not a bigger table. */
    uint32_t wanted = capacity_ == 0u ? first_capacity : capacity_;
    if (4u * (count_ + 1u) > wanted) wanted *= 2u;
    auto *const grown = static_cast<Entry *>(blocks_.allocate(sizeof(Entry) * wanted, Pool::paged));
    if (grown == nullptr) return Status::no_memory;
    for (uint32_t index = 0u; index < wanted; index++) grown[index] = Entry{0u, nullptr, 0u, false};
    Entry *const   previous = entries_;
    const uint32_t previous_capacity = capacity_;
    entries_ = grown;
    capacity_ = wanted;
    count_ = 0u;
    removed_ = 0u;
    for (uint32_t index = 0u; index < previous_capacity; index++) {
        const Entry &entry = previous[index];
        if (entry.id_ == 0u || entry.removed_) continue;
        *free_entry(entry.domain_, entry.id_) = entry;
        count_++;
    }
    if (previous != nullptr) blocks_.release(previous);
    return Status::ok;
}

}  // namespace av
