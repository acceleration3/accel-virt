#include "icd/d3d/buffer_slices.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/gpu_timeline.hpp"

namespace av::d3d {

BufferSlices::BufferSlices(BlockSource &memory, GpuTimeline &timeline, ThreadLock &lock)
    : timeline_(timeline), lock_(lock), ranges_(memory, large_alignment),
      free_{{memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged},
            {memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged},
            {memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged}},
      carved_{}, retired_(memory, Pool::paged), retired_first_(0u), outstanding_(0u), lost_(false)
{
    AV_SCOPE("BufferSlices::BufferSlices");
}

void BufferSlices::take(uint64_t bytes)
{
    AV_SCOPE("BufferSlices::take");
    const ThreadLockHold release(lock_);
    ranges_.reset(0u, bytes & ~(large_alignment - 1u));
    for (uint32_t which = 0u; which < small_classes; which++) {
        free_[which].clear();
        carved_[which] = 0u;
    }
    retired_.clear();
    retired_first_ = 0u;
    outstanding_ = 0u;
    lost_ = false;
}

Status BufferSlices::take_slice(uint64_t bytes, Slice &slice)
{
    AV_SCOPE("BufferSlices::take_slice");
    slice = Slice{0u, 0u};
    if (bytes == 0u || bytes > ranges_.end() - ranges_.begin()) return Status::invalid_parameter;
    const ThreadLockHold release(lock_);
    if (lost_) return Status::invalid_state;
    reclaim();
    const Status room = make_retired_room();
    if (room != Status::ok) return room;
    for (;;) {
        const Status taken = take_once(bytes, slice);
        if (taken == Status::ok) outstanding_++;
        if (taken != Status::no_space) return taken;
        if (retired() == 0u) return Status::no_space;
        if (!wait_oldest()) return Status::invalid_state;
    }
}

void BufferSlices::retire(const Slice &slice)
{
    AV_SCOPE("BufferSlices::retire");
    if (slice.bytes_ == 0u || ranges_.end() == 0u) return;
    const ThreadLockHold release(lock_);
    reclaim();
    if (outstanding_ != 0u) outstanding_--;
    static_cast<void>(retired_.append(Retired{slice.offset_, slice.bytes_, timeline_.recording()}));
}

void BufferSlices::give_back(const Slice &slice)
{
    AV_SCOPE("BufferSlices::give_back");
    if (slice.bytes_ == 0u || ranges_.end() == 0u) return;
    const ThreadLockHold release(lock_);
    if (outstanding_ != 0u) outstanding_--;
    free_slice(slice.offset_, slice.bytes_);
}

uint32_t BufferSlices::retired() const
{
    AV_SCOPE("BufferSlices::retired");
    return retired_.size() - retired_first_;
}

uint32_t BufferSlices::free_in_class(uint64_t bytes) const
{
    AV_SCOPE("BufferSlices::free_in_class");
    const uint32_t which = class_of(bytes);
    return which == no_class ? 0u : free_[which].size();
}

uint64_t BufferSlices::begin() const
{
    AV_SCOPE("BufferSlices::begin");
    return ranges_.begin();
}

uint64_t BufferSlices::end() const
{
    AV_SCOPE("BufferSlices::end");
    return ranges_.end();
}

uint32_t BufferSlices::class_of(uint64_t bytes) const
{
    AV_SCOPE("BufferSlices::class_of");
    if (bytes > largest_small) return no_class;
    uint32_t which = 0u;
    while (class_bytes(which) < bytes) which++;
    return which;
}

uint64_t BufferSlices::class_bytes(uint32_t which) const
{
    AV_SCOPE("BufferSlices::class_bytes");
    return smallest << which;
}

Status BufferSlices::take_once(uint64_t bytes, Slice &slice)
{
    AV_SCOPE("BufferSlices::take_once");
    const uint32_t which = class_of(bytes);
    if (which == no_class) {
        const uint64_t rounded = (bytes + large_alignment - 1u) & ~(large_alignment - 1u);
        uint64_t       offset = 0u;
        const Status   placed = ranges_.place(rounded, offset);
        if (placed != Status::ok) return placed;
        slice = Slice{offset, rounded};
        return Status::ok;
    }
    if (free_[which].size() == 0u) {
        const Status carved = carve(which);
        if (carved != Status::ok) return carved;
    }
    const uint32_t top = free_[which].size() - 1u;
    slice = Slice{free_[which].at(top), class_bytes(which)};
    free_[which].truncate(top);
    return Status::ok;
}

Status BufferSlices::carve(uint32_t which)
{
    AV_SCOPE("BufferSlices::carve");
    const uint64_t each = class_bytes(which);
    const uint32_t count = static_cast<uint32_t>(block_bytes / each);
    const Status room = free_[which].reserve(carved_[which] + count);
    if (room != Status::ok) return room;
    uint64_t     block = 0u;
    const Status placed = ranges_.place(block_bytes, block);
    if (placed != Status::ok) return placed;
    for (uint32_t index = count; index > 0u; index--)
        static_cast<void>(free_[which].append(block + (index - 1u) * each));
    carved_[which] += count;
    return Status::ok;
}

Status BufferSlices::make_retired_room()
{
    AV_SCOPE("BufferSlices::make_retired_room");
    return retired_.reserve(retired_.size() + outstanding_ + 1u);
}

void BufferSlices::compact_retired()
{
    AV_SCOPE("BufferSlices::compact_retired");
    if (retired_first_ == 0u || 2u * retired_first_ < retired_.size()) return;
    const uint32_t left = retired();
    for (uint32_t index = 0u; index < left; index++) retired_.at(index) = retired_.at(retired_first_ + index);
    retired_.truncate(left);
    retired_first_ = 0u;
}

void BufferSlices::reclaim()
{
    AV_SCOPE("BufferSlices::reclaim");
    if (retired() == 0u) return;
    const uint64_t done = timeline_.completed();
    while (retired() != 0u && retired_.at(retired_first_).tag_ <= done) {
        free_slice(retired_.at(retired_first_).offset_, retired_.at(retired_first_).bytes_);
        retired_first_++;
    }
    compact_retired();
}

void BufferSlices::free_slice(uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("BufferSlices::free_slice");
    const uint32_t which = class_of(bytes);
    if (which == no_class) {
        static_cast<void>(ranges_.remove(offset));
        return;
    }
    if (free_[which].size() < carved_[which]) static_cast<void>(free_[which].append(offset));
}

bool BufferSlices::wait_oldest()
{
    AV_SCOPE("BufferSlices::wait_oldest");
    if (!timeline_.wait(retired_.at(retired_first_).tag_)) {
        lost_ = true;
        return false;
    }
    reclaim();
    return true;
}

bool BufferSlices::invariants_hold() const
{
    AV_SCOPE("BufferSlices::invariants_hold");
    if (!ranges_.invariants_hold() || retired_first_ > retired_.size()) return false;
    if (retired_.capacity() < retired_.size() + outstanding_) return false;
    for (uint32_t which = 0u; which < small_classes; which++) {
        if (free_[which].size() > carved_[which]) return false;
        for (const uint64_t offset : free_[which])
            if (offset % class_bytes(which) != 0u || offset < ranges_.begin() || offset >= ranges_.end()) return false;
    }
    for (uint32_t index = retired_first_ + 1u; index < retired_.size(); index++)
        if (retired_.at(index).tag_ < retired_.at(index - 1u).tag_) return false;
    return true;
}

}
