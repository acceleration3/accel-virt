#include "executor/vulkan/handle_registry.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>

namespace av {

HandleRegistry::HandleRegistry()
    : table_(nullptr), handles_(nullptr), parents_(nullptr), children_(nullptr), free_(nullptr), index_(nullptr),
      kinds_(nullptr), buckets_(min_buckets), free_count_(0u), end_(0u), count_(0u)
{
    AV_SCOPE("HandleRegistry::HandleRegistry");
    static_assert(max_handles < no_slot);
    static_assert((index_capacity & (index_capacity - 1u)) == 0u);
}

HandleRegistry::~HandleRegistry()
{
    AV_SCOPE("HandleRegistry::~HandleRegistry");
    forget_all();
}

bool HandleRegistry::remember(uint64_t handle, uint64_t parent, HandleKind kind)
{
    AV_SCOPE("HandleRegistry::remember");
    /* The null handle is the API's own and is never anybody's to own. */
    if (handle == 0u) return true;
    if (knows(handle)) return true;
    if (count_ == max_handles || !reserve()) return false;
    if ((count_ + 1u) * 2u > buckets_) rebuild(buckets_ * 2u);
    const uint32_t parent_slot = parent != 0u ? slot_of(parent) : no_slot;
    const uint32_t slot = free_count_ != 0u ? free_[--free_count_] : end_++;
    handles_[slot] = handle;
    kinds_[slot] = kind;
    parents_[slot] = parent_slot;
    children_[slot] = 0u;
    if (parent_slot != no_slot) children_[parent_slot]++;
    index_[bucket_of(handle)] = slot + 1u;
    count_++;
    return true;
}

bool HandleRegistry::knows(uint64_t handle) const
{
    AV_SCOPE("HandleRegistry::knows");
    if (handle == 0u) return true;
    return slot_of(handle) != no_slot;
}

bool HandleRegistry::knows(uint64_t handle, HandleKind kind) const
{
    AV_SCOPE("HandleRegistry::knows");
    if (handle == 0u) return true;
    const uint32_t slot = slot_of(handle);
    return slot != no_slot && kinds_[slot] == kind;
}

void HandleRegistry::forget(uint64_t handle)
{
    AV_SCOPE("HandleRegistry::forget");
    if (handle == 0u) return;
    const uint32_t slot = slot_of(handle);
    if (slot == no_slot) return;
    const bool had_children = children_[slot] != 0u;
    free_slot(slot);
    if (had_children) sweep_orphans();
}

void HandleRegistry::forget_children(uint64_t handle)
{
    AV_SCOPE("HandleRegistry::forget_children");
    if (handle == 0u) return;
    const uint32_t slot = slot_of(handle);
    if (slot == no_slot || children_[slot] == 0u) return;
    bool grandchildren = false;
    for (uint32_t i = 0u; i < end_ && children_[slot] != 0u; i++) {
        if (handles_[i] == 0u || parents_[i] != slot) continue;
        grandchildren = grandchildren || children_[i] != 0u;
        free_slot(i);
    }
    if (grandchildren) sweep_orphans();
}

void HandleRegistry::forget_all()
{
    AV_SCOPE("HandleRegistry::forget_all");
    if (table_ != nullptr) static_cast<void>(munmap(table_, table_bytes));
    table_ = nullptr;
    handles_ = nullptr;
    parents_ = nullptr;
    children_ = nullptr;
    free_ = nullptr;
    index_ = nullptr;
    kinds_ = nullptr;
    buckets_ = min_buckets;
    free_count_ = 0u;
    end_ = 0u;
    count_ = 0u;
}

bool HandleRegistry::reserve()
{
    AV_SCOPE("HandleRegistry::reserve");
    if (table_ != nullptr) return true;
    void *const mapped = mmap(nullptr, table_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                              -1, 0);
    if (mapped == MAP_FAILED) return false;
    table_ = mapped;
    handles_ = static_cast<uint64_t *>(mapped);
    parents_ = reinterpret_cast<uint32_t *>(handles_ + max_handles);
    children_ = parents_ + max_handles;
    free_ = children_ + max_handles;
    index_ = free_ + max_handles;
    kinds_ = reinterpret_cast<HandleKind *>(index_ + index_capacity);
    return true;
}

uint32_t HandleRegistry::count() const
{
    AV_SCOPE("HandleRegistry::count");
    return count_;
}

uint32_t HandleRegistry::slot_of(uint64_t handle) const
{
    AV_SCOPE("HandleRegistry::slot_of");
    if (table_ == nullptr) return no_slot;
    const uint32_t entry = index_[bucket_of(handle)];
    return entry != 0u ? entry - 1u : no_slot;
}

uint32_t HandleRegistry::bucket_of(uint64_t handle) const
{
    AV_SCOPE("HandleRegistry::bucket_of");
    const uint32_t mask = buckets_ - 1u;
    uint32_t       bucket = home_of(handle);
    while (index_[bucket] != 0u && handles_[index_[bucket] - 1u] != handle) bucket = (bucket + 1u) & mask;
    return bucket;
}

uint32_t HandleRegistry::home_of(uint64_t handle) const
{
    AV_SCOPE("HandleRegistry::home_of");
    return static_cast<uint32_t>((handle * spread) >> 32u) & (buckets_ - 1u);
}

void HandleRegistry::unindex(uint32_t bucket)
{
    AV_SCOPE("HandleRegistry::unindex");
    const uint32_t mask = buckets_ - 1u;
    uint32_t       hole = bucket;
    for (uint32_t next = (hole + 1u) & mask; index_[next] != 0u; next = (next + 1u) & mask) {
        /* An entry moves into the hole only if the hole is on its probe path:
         * between its home bucket and where it sits now. */
        const uint32_t home = home_of(handles_[index_[next] - 1u]);
        if (((next - home) & mask) < ((next - hole) & mask)) continue;
        index_[hole] = index_[next];
        hole = next;
    }
    index_[hole] = 0u;
}

void HandleRegistry::rebuild(uint32_t buckets)
{
    AV_SCOPE("HandleRegistry::rebuild");
    buckets_ = buckets;
    for (uint32_t bucket = 0u; bucket < buckets_; bucket++) index_[bucket] = 0u;
    for (uint32_t slot = 0u; slot < end_; slot++) {
        if (handles_[slot] != 0u) index_[bucket_of(handles_[slot])] = slot + 1u;
    }
}

void HandleRegistry::free_slot(uint32_t slot)
{
    AV_SCOPE("HandleRegistry::free_slot");
    unindex(bucket_of(handles_[slot]));
    if (parents_[slot] != no_slot) children_[parents_[slot]]--;
    handles_[slot] = 0u;
    parents_[slot] = no_slot;
    free_[free_count_++] = slot;
    count_--;
}

void HandleRegistry::sweep_orphans()
{
    AV_SCOPE("HandleRegistry::sweep_orphans");
    bool freed = true;
    while (freed) {
        freed = false;
        for (uint32_t i = 0u; i < end_; i++) {
            if (handles_[i] == 0u || parents_[i] == no_slot) continue;
            if (handles_[parents_[i]] != 0u) continue;
            free_slot(i);
            freed = true;
        }
    }
}

bool HandleRegistry::invariants_hold() const
{
    AV_SCOPE("HandleRegistry::invariants_hold");
    if (table_ == nullptr) return end_ == 0u && count_ == 0u && free_count_ == 0u && buckets_ == min_buckets;
    if (end_ > max_handles || count_ + free_count_ != end_) return false;
    if (buckets_ < min_buckets || buckets_ > index_capacity || (buckets_ & (buckets_ - 1u)) != 0u) return false;
    if (count_ * 2u > buckets_) return false;
    for (uint32_t i = 0u; i < free_count_; i++) {
        if (free_[i] >= end_ || handles_[free_[i]] != 0u) return false;
    }
    uint32_t held = 0u;
    uint32_t made_through = 0u;
    uint32_t claimed = 0u;
    for (uint32_t i = 0u; i < end_; i++) {
        const uint32_t parent = parents_[i];
        if (handles_[i] == 0u) {
            if (parent != no_slot) return false;
            continue;
        }
        held++;
        claimed += children_[i];
        if (slot_of(handles_[i]) != i) return false;
        if (parent == no_slot) continue;
        if (parent >= end_ || parent == i || handles_[parent] == 0u) return false;
        made_through++;
    }
    uint32_t indexed = 0u;
    for (uint32_t bucket = 0u; bucket < buckets_; bucket++) indexed += index_[bucket] != 0u ? 1u : 0u;
    return held == count_ && indexed == count_ && claimed == made_through;
}

}  // namespace av
