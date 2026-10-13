#include "icd/d3d/buffer_slices.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "icd/d3d/gpu_timeline.hpp"

namespace av::d3d {

BufferSlices::BufferSlices(GpuTimeline &timeline, icd::ThreadLock &lock)
    : timeline_(timeline), lock_(lock), ranges_(large_alignment), free_{}, free_count_{}, carved_{}, retired_{},
      retired_first_(0u), retired_count_(0u), lost_(false)
{
    AV_SCOPE("BufferSlices::BufferSlices");
}

void BufferSlices::take(uint64_t begin, uint64_t end)
{
    AV_SCOPE("BufferSlices::take");
    const icd::ThreadLockHold release(lock_);
    ranges_.reset(begin, end);
    for (uint32_t which = 0u; which < small_classes; which++) {
        free_count_[which] = 0u;
        carved_[which] = 0u;
    }
    retired_first_ = 0u;
    retired_count_ = 0u;
    lost_ = false;
}

Status BufferSlices::take_slice(uint64_t bytes, Slice &slice)
{
    AV_SCOPE("BufferSlices::take_slice");
    slice = Slice{0u, 0u};
    if (bytes == 0u || bytes > ranges_.end() - ranges_.begin()) return Status::invalid_parameter;
    const icd::ThreadLockHold release(lock_);
    if (lost_) return Status::invalid_state;
    reclaim();
    for (;;) {
        const Status taken = take_once(bytes, slice);
        if (taken != Status::no_space) return taken;
        if (retired_count_ == 0u) return Status::no_space;
        if (!wait_oldest()) return Status::invalid_state;
    }
}

void BufferSlices::retire(const Slice &slice)
{
    AV_SCOPE("BufferSlices::retire");
    if (slice.bytes == 0u) return;
    const icd::ThreadLockHold release(lock_);
    reclaim();
    if (retired_count_ == max_retired && !wait_oldest()) return;   /* a lost session's memory is not reused */
    retired_[(retired_first_ + retired_count_) % max_retired] = Retired{slice.offset, slice.bytes,
                                                                        timeline_.recording()};
    retired_count_++;
}

void BufferSlices::give_back(const Slice &slice)
{
    AV_SCOPE("BufferSlices::give_back");
    if (slice.bytes == 0u) return;
    const icd::ThreadLockHold release(lock_);
    free_slice(slice.offset, slice.bytes);
}

uint32_t BufferSlices::retired() const
{
    AV_SCOPE("BufferSlices::retired");
    return retired_count_;
}

uint32_t BufferSlices::free_in_class(uint64_t bytes) const
{
    AV_SCOPE("BufferSlices::free_in_class");
    const uint32_t which = class_of(bytes);
    return which == no_class ? 0u : free_count_[which];
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
    if (free_count_[which] == 0u) {
        const Status carved = carve(which);
        if (carved != Status::ok) return carved;
    }
    slice = Slice{free_[which][--free_count_[which]], class_bytes(which)};
    return Status::ok;
}

Status BufferSlices::carve(uint32_t which)
{
    AV_SCOPE("BufferSlices::carve");
    const uint64_t each = class_bytes(which);
    const uint32_t count = static_cast<uint32_t>(block_bytes / each);
    if (carved_[which] + count > max_per_class) return Status::no_space;
    uint64_t     block = 0u;
    const Status placed = ranges_.place(block_bytes, block);
    if (placed != Status::ok) return placed;
    /* Pushed highest first, so the block is handed out from its start. */
    for (uint32_t index = count; index > 0u; index--) free_[which][free_count_[which]++] = block + (index - 1u) * each;
    carved_[which] += count;
    return Status::ok;
}

void BufferSlices::reclaim()
{
    AV_SCOPE("BufferSlices::reclaim");
    if (retired_count_ == 0u) return;
    const uint64_t done = timeline_.completed();
    while (retired_count_ != 0u && retired_[retired_first_].tag <= done) {
        free_slice(retired_[retired_first_].offset, retired_[retired_first_].bytes);
        retired_first_ = (retired_first_ + 1u) % max_retired;
        retired_count_--;
    }
}

void BufferSlices::free_slice(uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("BufferSlices::free_slice");
    const uint32_t which = class_of(bytes);
    if (which == no_class) {
        static_cast<void>(ranges_.remove(offset));
        return;
    }
    /* A class never holds more slices than it carved, so the stack has room. */
    if (free_count_[which] < max_per_class) free_[which][free_count_[which]++] = offset;
}

bool BufferSlices::wait_oldest()
{
    AV_SCOPE("BufferSlices::wait_oldest");
    if (retired_count_ == 0u) return false;
    if (!timeline_.wait(retired_[retired_first_].tag)) {
        lost_ = true;
        return false;
    }
    reclaim();
    return true;
}

bool BufferSlices::invariants_hold() const
{
    AV_SCOPE("BufferSlices::invariants_hold");
    if (!ranges_.invariants_hold() || retired_count_ > max_retired || retired_first_ >= max_retired) return false;
    for (uint32_t which = 0u; which < small_classes; which++) {
        if (carved_[which] > max_per_class || free_count_[which] > carved_[which]) return false;
        for (uint32_t index = 0u; index < free_count_[which]; index++)
            if (free_[which][index] % class_bytes(which) != 0u || free_[which][index] < ranges_.begin() ||
                free_[which][index] >= ranges_.end())
                return false;
    }
    for (uint32_t index = 1u; index < retired_count_; index++)
        if (retired_[(retired_first_ + index) % max_retired].tag <
            retired_[(retired_first_ + index - 1u) % max_retired].tag)
            return false;
    return true;
}

}  // namespace av::d3d
