#include "icd/d3d/buffer_slices.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/d3d/gpu_timeline.hpp"

namespace av::d3d {

BufferSlices::BufferSlices(BlockSource &memory, icd::HostChannel &channel, const void *owner, GpuTimeline &timeline,
                           ThreadLock &lock, PollClock &clock, DriverLog &lines)
    : memory_(memory), channel_(channel), owner_(owner), timeline_(timeline), lock_(lock),
      blocks_(memory, channel, owner, clock, lines),
      free_{{memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged},
            {memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged},
            {memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged}},
      carved_{}, retired_(memory, Pool::paged), retired_first_(0u), outstanding_(0u), lost_(false)
{
    AV_SCOPE("BufferSlices::BufferSlices");
}

BufferSlices::~BufferSlices()
{
    AV_SCOPE("BufferSlices::~BufferSlices");
}

Status BufferSlices::take_slice(uint64_t bytes, Slice &slice)
{
    AV_SCOPE("BufferSlices::take_slice");
    slice = Slice{0u, 0u};
    if (bytes == 0u || bytes > SliceBlocks::largest_block_bytes) return Status::invalid_parameter;
    const ThreadLockHold release(lock_);
    if (lost_) return Status::invalid_state;
    reclaim();
    const Status room = make_retired_room();
    if (room != Status::ok) return room;
    uint32_t fresh = SliceBlocks::slot_count;
    for (;;) {
        const Status taken = take_once(bytes, slice);
        if (taken == Status::ok) outstanding_++;
        if (taken != Status::no_space) {
            if (taken != Status::ok) static_cast<void>(blocks_.close_unused(fresh));
            return taken;
        }
        if (blocks_.open(bytes, fresh) == Status::ok) continue;
        if (retired() == 0u) return Status::no_space;
        if (!wait_oldest()) return Status::invalid_state;
    }
}

void BufferSlices::retire(const Slice &slice)
{
    AV_SCOPE("BufferSlices::retire");
    if (slice.bytes_ == 0u) return;
    const ThreadLockHold release(lock_);
    reclaim();
    if (outstanding_ != 0u) outstanding_--;
    static_cast<void>(retired_.append(Retired{slice.offset_, slice.bytes_, timeline_.recording()}));
}

void BufferSlices::give_back(const Slice &slice)
{
    AV_SCOPE("BufferSlices::give_back");
    if (slice.bytes_ == 0u) return;
    const ThreadLockHold release(lock_);
    if (outstanding_ != 0u) outstanding_--;
    free_slice(slice.offset_, slice.bytes_);
}

void BufferSlices::trim()
{
    AV_SCOPE("BufferSlices::trim");
    const ThreadLockHold release(lock_);
    reclaim();
    uint32_t slot = 0u;
    while (blocks_.next_to_close(slot)) {
        forget_free_in(slot);
        blocks_.close(slot);
    }
}

uint8_t *BufferSlices::view(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::view");
    const ThreadLockHold release(lock_);
    return blocks_.view(slice.offset_);
}

uint64_t BufferSlices::memory(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::memory");
    const ThreadLockHold release(lock_);
    return blocks_.handle(slice.offset_);
}

uint64_t BufferSlices::offset_in_block(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::offset_in_block");
    return blocks_.within(slice.offset_);
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

uint32_t BufferSlices::open_blocks() const
{
    AV_SCOPE("BufferSlices::open_blocks");
    return blocks_.open_count();
}

uint64_t BufferSlices::open_bytes() const
{
    AV_SCOPE("BufferSlices::open_bytes");
    return blocks_.open_bytes();
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
        const Status   placed = blocks_.place(rounded, offset);
        if (placed != Status::ok) return placed;
        blocks_.took(offset);
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
    blocks_.took(slice.offset_);
    return Status::ok;
}

Status BufferSlices::carve(uint32_t which)
{
    AV_SCOPE("BufferSlices::carve");
    const uint64_t each = class_bytes(which);
    const uint32_t count = static_cast<uint32_t>(chunk_bytes / each);
    const Status room = free_[which].reserve(carved_[which] + count);
    if (room != Status::ok) return room;
    uint64_t     chunk = 0u;
    const Status placed = blocks_.place(chunk_bytes, chunk);
    if (placed != Status::ok) return placed;
    for (uint32_t index = count; index > 0u; index--)
        static_cast<void>(free_[which].append(chunk + (index - 1u) * each));
    carved_[which] += count;
    return Status::ok;
}

void BufferSlices::forget_free_in(uint32_t slot)
{
    AV_SCOPE("BufferSlices::forget_free_in");
    for (uint32_t which = 0u; which < small_classes; which++) {
        uint32_t kept = 0u;
        for (uint32_t index = 0u; index < free_[which].size(); index++) {
            const uint64_t offset = free_[which].at(index);
            if (blocks_.slot_of(offset) != slot) free_[which].at(kept++) = offset;
        }
        carved_[which] -= free_[which].size() - kept;
        free_[which].truncate(kept);
    }
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
    if (!blocks_.holds(blocks_.slot_of(offset))) return;
    const uint32_t which = class_of(bytes);
    if (which == no_class) blocks_.remove(offset);
    else if (free_[which].size() < carved_[which]) static_cast<void>(free_[which].append(offset));
    blocks_.freed(offset);
}

bool BufferSlices::wait_oldest()
{
    AV_SCOPE("BufferSlices::wait_oldest");
    channel_.transport().note(ChannelCounter::slice_waits, 1u);
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
    if (retired_first_ > retired_.size() || retired_.capacity() < retired_.size() + outstanding_) return false;
    if (!blocks_.invariants_hold()) return false;
    for (uint32_t which = 0u; which < small_classes; which++) {
        if (free_[which].size() > carved_[which]) return false;
        for (const uint64_t offset : free_[which]) {
            if (offset % class_bytes(which) != 0u || blocks_.within(offset) >= blocks_.bytes_of(blocks_.slot_of(offset)))
                return false;
        }
    }
    for (uint32_t index = retired_first_ + 1u; index < retired_.size(); index++)
        if (retired_.at(index).tag_ < retired_.at(index - 1u).tag_) return false;
    return true;
}

}
