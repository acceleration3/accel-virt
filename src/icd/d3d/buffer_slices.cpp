#include "icd/d3d/buffer_slices.hpp"

#include "core/platform/poll_clock.hpp"
#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/d3d/gpu_timeline.hpp"
#include "proto/resources/host_memory_request.hpp"

namespace av::d3d {

static_assert(BufferSlices::slot_bytes == BufferSlices::largest_block_bytes, "a block fits one slot");
static_assert(BufferSlices::slot_count * BufferSlices::slot_bytes >= BufferSlices::ceiling_bytes,
              "the slots cover the ceiling");

BufferSlices::BufferSlices(BlockSource &memory, icd::HostChannel &channel, const void *owner, GpuTimeline &timeline,
                           ThreadLock &lock, PollClock &clock)
    : memory_(memory), channel_(channel), owner_(owner), timeline_(timeline), lock_(lock), clock_(clock), blocks_{},
      total_bytes_(0u),
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
    for (ObjectStorage<SliceBlock> &block : blocks_) block.destroy();
}

Status BufferSlices::take_slice(uint64_t bytes, Slice &slice)
{
    AV_SCOPE("BufferSlices::take_slice");
    slice = Slice{0u, 0u};
    if (bytes == 0u || bytes > largest_block_bytes) return Status::invalid_parameter;
    const ThreadLockHold release(lock_);
    if (lost_) return Status::invalid_state;
    reclaim();
    const Status room = make_retired_room();
    if (room != Status::ok) return room;
    for (;;) {
        const Status taken = take_once(bytes, slice);
        if (taken == Status::ok) outstanding_++;
        if (taken != Status::no_space) return taken;
        if (open_block(bytes) == Status::ok) continue;
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
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (!blocks_[slot].constructed() || !blocks_[slot].object().empty()) continue;
        if (clock_.now_nanoseconds() - blocks_[slot].object().emptied_at() >= spare_lifetime_nanoseconds)
            close_block(slot);
    }
}

uint8_t *BufferSlices::view(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::view");
    const ThreadLockHold release(lock_);
    return blocks_[slice.offset_ >> slot_shift].object().view() + offset_in_block(slice);
}

uint64_t BufferSlices::memory(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::memory");
    const ThreadLockHold release(lock_);
    return blocks_[slice.offset_ >> slot_shift].object().handle();
}

uint64_t BufferSlices::offset_in_block(const Slice &slice) const
{
    AV_SCOPE("BufferSlices::offset_in_block");
    return slice.offset_ & (slot_bytes - 1u);
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
    uint32_t open = 0u;
    for (const ObjectStorage<SliceBlock> &block : blocks_) open += block.constructed() ? 1u : 0u;
    return open;
}

uint64_t BufferSlices::open_bytes() const
{
    AV_SCOPE("BufferSlices::open_bytes");
    return total_bytes_;
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
        const Status   placed = place_in_blocks(rounded, offset);
        if (placed != Status::ok) return placed;
        blocks_[offset >> slot_shift].object().took();
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
    blocks_[slice.offset_ >> slot_shift].object().took();
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
    const Status placed = place_in_blocks(chunk_bytes, chunk);
    if (placed != Status::ok) return placed;
    for (uint32_t index = count; index > 0u; index--)
        static_cast<void>(free_[which].append(chunk + (index - 1u) * each));
    carved_[which] += count;
    return Status::ok;
}

Status BufferSlices::place_in_blocks(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("BufferSlices::place_in_blocks");
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        uint64_t within = 0u;
        if (!blocks_[slot].constructed() || blocks_[slot].object().place(bytes, within) != Status::ok) continue;
        offset = (uint64_t{slot} << slot_shift) | within;
        return Status::ok;
    }
    return Status::no_space;
}

uint64_t BufferSlices::next_block_bytes(uint64_t bytes) const
{
    AV_SCOPE("BufferSlices::next_block_bytes");
    uint64_t grown = first_block_bytes;
    for (uint32_t open = open_blocks(); open != 0u && grown < largest_block_bytes; open--) grown *= 2u;
    const uint64_t needed = (bytes + large_alignment - 1u) & ~(large_alignment - 1u);
    return needed > grown ? needed : grown;
}

Status BufferSlices::open_block(uint64_t bytes)
{
    AV_SCOPE("BufferSlices::open_block");
    const uint64_t size = next_block_bytes(bytes);
    if (size > largest_block_bytes || total_bytes_ + size > ceiling_bytes) return Status::no_space;
    uint32_t slot = 0u;
    while (slot < slot_count && blocks_[slot].constructed()) slot++;
    if (slot == slot_count) return Status::no_space;
    HostAllocation *made = nullptr;
    const Status    opened =
        channel_.open_memory(size, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), owner_, made);
    if (opened != Status::ok) return opened;
    const uint64_t usable = made->bytes() < slot_bytes ? made->bytes() : slot_bytes;
    const Status   built = blocks_[slot].construct(memory_, *made, usable, large_alignment);
    if (built != Status::ok) {
        static_cast<void>(channel_.close_memory(*made));
        return built;
    }
    total_bytes_ += blocks_[slot].object().bytes();
    return Status::ok;
}

void BufferSlices::close_block(uint32_t slot)
{
    AV_SCOPE("BufferSlices::close_block");
    for (uint32_t which = 0u; which < small_classes; which++) {
        uint32_t kept = 0u;
        for (uint32_t index = 0u; index < free_[which].size(); index++) {
            const uint64_t offset = free_[which].at(index);
            if ((offset >> slot_shift) != slot) free_[which].at(kept++) = offset;
        }
        carved_[which] -= free_[which].size() - kept;
        free_[which].truncate(kept);
    }
    HostAllocation *const allocation = blocks_[slot].object().allocation();
    total_bytes_ -= blocks_[slot].object().bytes();
    blocks_[slot].destroy();
    static_cast<void>(channel_.close_memory(*allocation));
}

void BufferSlices::note_empty(uint32_t slot)
{
    AV_SCOPE("BufferSlices::note_empty");
    if (has_spare(slot)) close_block(slot);
    else blocks_[slot].object().empty_since(clock_.now_nanoseconds());
}

bool BufferSlices::has_spare(uint32_t except) const
{
    AV_SCOPE("BufferSlices::has_spare");
    for (uint32_t slot = 0u; slot < slot_count; slot++)
        if (slot != except && blocks_[slot].constructed() && blocks_[slot].object().empty()) return true;
    return false;
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
    const uint32_t slot = static_cast<uint32_t>(offset >> slot_shift);
    if (slot >= slot_count || !blocks_[slot].constructed()) return;
    SliceBlock    &block = blocks_[slot].object();
    const uint32_t which = class_of(bytes);
    if (which == no_class) block.remove(offset & (slot_bytes - 1u));
    else if (free_[which].size() < carved_[which]) static_cast<void>(free_[which].append(offset));
    block.freed();
    if (block.live() == 0u && !block.empty()) note_empty(slot);
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
    uint64_t total = 0u;
    uint32_t spares = 0u;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (!blocks_[slot].constructed()) continue;
        const SliceBlock &block = blocks_[slot].object();
        if (!block.invariants_hold() || block.bytes() > slot_bytes) return false;
        total += block.bytes();
        spares += block.empty() ? 1u : 0u;
    }
    if (total != total_bytes_ || total > ceiling_bytes || spares > 1u) return false;
    for (uint32_t which = 0u; which < small_classes; which++) {
        if (free_[which].size() > carved_[which]) return false;
        for (const uint64_t offset : free_[which]) {
            const uint32_t slot = static_cast<uint32_t>(offset >> slot_shift);
            if (offset % class_bytes(which) != 0u || slot >= slot_count || !blocks_[slot].constructed() ||
                (offset & (slot_bytes - 1u)) >= blocks_[slot].object().bytes())
                return false;
        }
    }
    for (uint32_t index = retired_first_ + 1u; index < retired_.size(); index++)
        if (retired_.at(index).tag_ < retired_.at(index - 1u).tag_) return false;
    return true;
}

}
