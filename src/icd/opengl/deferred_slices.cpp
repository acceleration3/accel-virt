#include "icd/opengl/deferred_slices.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/slice_groups.hpp"
#include "icd/opengl/sliced_buffers.hpp"

namespace av::glicd {

DeferredSlices::DeferredSlices(BlockSource &memory) : entries_(memory, Pool::paged), marks_(memory, Pool::paged)
{
    AV_SCOPE("DeferredSlices::DeferredSlices");
}

Status DeferredSlices::add(const SliceGroups &groups, uint32_t group, const SlicedBuffers *owner, const Entry &entry)
{
    AV_SCOPE("DeferredSlices::add");
    uint32_t others = 0u;
    uint32_t cursor = 0u;
    for (const SliceGroups::Member *member = groups.next_member(group, cursor); member != nullptr;
         member = groups.next_member(group, cursor))
        others += member->member_ != owner ? 1u : 0u;
    const Status entry_room = entries_.reserve(entries_.size() + 1u);
    if (entry_room != Status::ok) return entry_room;
    const Status mark_room = marks_.reserve(marks_.size() + others);
    if (mark_room != Status::ok) return mark_room;
    Entry kept = entry;
    kept.marks_ = others;
    static_cast<void>(entries_.append(kept));
    cursor = 0u;
    for (const SliceGroups::Member *member = groups.next_member(group, cursor); member != nullptr;
         member = groups.next_member(group, cursor))
        if (member->member_ != owner)
            static_cast<void>(marks_.append(Mark{member->id_, member->member_->covering()}));
    return Status::ok;
}

bool DeferredSlices::covered(const SliceGroups &groups, uint32_t first, uint32_t count) const
{
    AV_SCOPE("DeferredSlices::covered");
    for (uint32_t index = first; index < first + count; index++) {
        const Mark &mark = marks_.at(index);
        const SlicedBuffers *const member = groups.member_of(mark.member_);
        if (member != nullptr && member->completed() < mark.fence_) return false;
    }
    return true;
}

bool DeferredSlices::take_ready(const SliceGroups &groups, Entry &ready)
{
    AV_SCOPE("DeferredSlices::take_ready");
    uint32_t first = 0u;
    for (uint32_t index = 0u; index < entries_.size(); index++) {
        const Entry &entry = entries_.at(index);
        if (covered(groups, first, entry.marks_)) {
            ready = entry;
            for (uint32_t mark = 0u; mark < ready.marks_; mark++) marks_.remove_at(first);
            entries_.remove_at(index);
            return true;
        }
        first += entry.marks_;
    }
    return false;
}

uint32_t DeferredSlices::pending() const
{
    AV_SCOPE("DeferredSlices::pending");
    return entries_.size();
}

void DeferredSlices::clear()
{
    AV_SCOPE("DeferredSlices::clear");
    entries_.clear();
    marks_.clear();
}

bool DeferredSlices::invariants_hold() const
{
    AV_SCOPE("DeferredSlices::invariants_hold");
    uint64_t counted = 0u;
    for (const Entry &entry : entries_) counted += entry.marks_;
    return counted == marks_.size() && entries_.invariants_hold() && marks_.invariants_hold();
}

}
