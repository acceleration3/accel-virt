#include "icd/opengl/slice_groups.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {
constexpr uint32_t group_shift = 32u;
}

SliceGroups::SliceGroups(BlockSource &memory, ThreadLock &lock)
    : lock_(lock), rows_(memory, Pool::paged), parents_(memory, Pool::paged), members_(memory, Pool::paged), next_(1u)
{
    AV_SCOPE("SliceGroups::SliceGroups");
}

ThreadLock &SliceGroups::lock()
{
    AV_SCOPE("SliceGroups::lock");
    return lock_;
}

uint32_t SliceGroups::mint()
{
    AV_SCOPE("SliceGroups::mint");
    const ThreadLockHold hold(lock_);
    return next_++;
}

uint32_t SliceGroups::root_of(uint32_t token) const
{
    AV_SCOPE("SliceGroups::root_of");
    const uint32_t *parent = parents_.find(token);
    for (uint32_t step = 0u; parent != nullptr && step <= parents_.count(); step++) {
        token = *parent;
        parent = parents_.find(token);
    }
    return token;
}

uint64_t SliceGroups::key_of(uint32_t group, uint32_t buffer) const
{
    AV_SCOPE("SliceGroups::key_of");
    return (static_cast<uint64_t>(root_of(group)) << group_shift) | buffer;
}

bool SliceGroups::placed_under(uint32_t root) const
{
    AV_SCOPE("SliceGroups::placed_under");
    bool placed = false;
    rows_.for_each([&placed, root](uint64_t key, const Row &) {
        placed = placed || static_cast<uint32_t>(key >> group_shift) == root;
    });
    return placed;
}

Status SliceGroups::admit_join(uint32_t joiner)
{
    AV_SCOPE("SliceGroups::admit_join");
    const ThreadLockHold hold(lock_);
    if (placed_under(root_of(joiner))) return Status::invalid_state;
    return parents_.reserve(parents_.count() + 1u);
}

Status SliceGroups::join(uint32_t joiner, uint32_t target)
{
    AV_SCOPE("SliceGroups::join");
    const ThreadLockHold hold(lock_);
    const uint32_t       from = root_of(joiner);
    const uint32_t       into = root_of(target);
    if (from == into) return Status::ok;
    if (placed_under(from)) return Status::invalid_state;
    uint32_t slot = 0u;
    return parents_.insert(from, into, slot);
}

bool SliceGroups::same(uint32_t first, uint32_t second) const
{
    AV_SCOPE("SliceGroups::same");
    const ThreadLockHold hold(lock_);
    return root_of(first) == root_of(second);
}

Status SliceGroups::insert(uint32_t group, uint32_t buffer, const Row &row)
{
    AV_SCOPE("SliceGroups::insert");
    if (buffer == 0u || rows_.find(key_of(group, buffer)) != nullptr) return Status::conflict;
    uint32_t slot = 0u;
    return rows_.insert(key_of(group, buffer), row, slot);
}

SliceGroups::Row *SliceGroups::find(uint32_t group, uint32_t buffer)
{
    AV_SCOPE("SliceGroups::find");
    return buffer == 0u ? nullptr : rows_.find(key_of(group, buffer));
}

bool SliceGroups::take(uint32_t group, uint32_t buffer, Row &row)
{
    AV_SCOPE("SliceGroups::take");
    return buffer != 0u && rows_.take(key_of(group, buffer), row);
}

void SliceGroups::drop_owner(const SlicedBuffers *owner)
{
    AV_SCOPE("SliceGroups::drop_owner");
    static_cast<void>(rows_.erase_if([owner](uint64_t, const Row &row) { return row.owner_ == owner; }));
}

Status SliceGroups::enroll(SlicedBuffers *member, uint32_t group)
{
    AV_SCOPE("SliceGroups::enroll");
    const uint64_t key = reinterpret_cast<uintptr_t>(member);
    if (members_.find(key) != nullptr) return Status::ok;
    uint32_t slot = 0u;
    return members_.insert(key, Member{member, group}, slot);
}

void SliceGroups::withdraw(const SlicedBuffers *member)
{
    AV_SCOPE("SliceGroups::withdraw");
    static_cast<void>(members_.erase(reinterpret_cast<uintptr_t>(member)));
}

bool SliceGroups::enrolled(const SlicedBuffers *member) const
{
    AV_SCOPE("SliceGroups::enrolled");
    return members_.find(reinterpret_cast<uintptr_t>(member)) != nullptr;
}

SlicedBuffers *SliceGroups::next_member(uint32_t group, uint32_t &cursor) const
{
    AV_SCOPE("SliceGroups::next_member");
    const uint32_t root = root_of(group);
    for (uint32_t slot = members_.next_live(cursor); slot != members_.no_slot; slot = members_.next_live(slot + 1u)) {
        const Member *const found = members_.at(slot);
        if (root_of(found->group_) != root) continue;
        cursor = slot + 1u;
        return found->member_;
    }
    cursor = members_.no_slot;
    return nullptr;
}

uint32_t SliceGroups::owned_by(const SlicedBuffers *owner) const
{
    AV_SCOPE("SliceGroups::owned_by");
    uint32_t owned = 0u;
    rows_.for_each([&owned, owner](uint64_t, const Row &row) { owned += row.owner_ == owner ? 1u : 0u; });
    return owned;
}

uint32_t SliceGroups::count() const
{
    AV_SCOPE("SliceGroups::count");
    return rows_.count();
}

bool SliceGroups::invariants_hold() const
{
    AV_SCOPE("SliceGroups::invariants_hold");
    bool sound = next_ != 0u;
    rows_.for_each([&sound](uint64_t key, const Row &row) {
        sound = sound && static_cast<uint32_t>(key) != 0u && row.owner_ != nullptr && row.size_ != 0u &&
                row.size_ <= row.slice_.bytes_ && row.map_offset_ + row.map_bytes_ <= row.size_ &&
                (row.access_ != 0u || row.map_bytes_ == 0u);
    });
    parents_.for_each([this, &sound](uint32_t token, const uint32_t &parent) {
        sound = sound && token != parent && root_of(parent) != token;
    });
    members_.for_each([&sound](uint64_t key, const Member &found) {
        sound = sound && found.member_ != nullptr && key == reinterpret_cast<uintptr_t>(found.member_);
    });
    return sound && rows_.invariants_hold() && parents_.invariants_hold() && members_.invariants_hold();
}

}
