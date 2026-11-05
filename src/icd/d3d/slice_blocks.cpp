#include "icd/d3d/slice_blocks.hpp"

#include "core/log/driver_log.hpp"
#include "core/platform/poll_clock.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/resources/host_memory_request.hpp"

namespace av::d3d {

static_assert(SliceBlocks::slot_bytes == SliceBlocks::largest_block_bytes, "a block fits one slot");
static_assert(SliceBlocks::slot_count * SliceBlocks::slot_bytes >= SliceBlocks::ceiling_bytes,
              "the slots cover the ceiling");

SliceBlocks::SliceBlocks(BlockSource &memory, icd::HostChannel &channel, const void *owner, PollClock &clock,
                         DriverLog &lines)
    : memory_(memory), channel_(channel), owner_(owner), clock_(clock), lines_(lines), blocks_{}, total_bytes_(0u),
      reopen_at_(0u), close_reported_(false)
{
    AV_SCOPE("SliceBlocks::SliceBlocks");
}

SliceBlocks::~SliceBlocks()
{
    AV_SCOPE("SliceBlocks::~SliceBlocks");
    for (ObjectStorage<SliceBlock> &block : blocks_) block.destroy();
}

Status SliceBlocks::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("SliceBlocks::place");
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        uint64_t inside = 0u;
        if (!blocks_[slot].constructed() || blocks_[slot].object().place(bytes, inside) != Status::ok) continue;
        offset = (uint64_t{slot} << slot_shift) | inside;
        return Status::ok;
    }
    return Status::no_space;
}

uint64_t SliceBlocks::next_block_bytes(uint64_t bytes) const
{
    AV_SCOPE("SliceBlocks::next_block_bytes");
    uint64_t grown = first_block_bytes;
    for (uint32_t opened = open_count(); opened != 0u && grown < largest_block_bytes; opened--) grown *= 2u;
    const uint64_t needed = (bytes + large_alignment - 1u) & ~(large_alignment - 1u);
    return needed > grown ? needed : grown;
}

Status SliceBlocks::open(uint64_t bytes, uint32_t &slot)
{
    AV_SCOPE("SliceBlocks::open");
    slot = none;
    const uint64_t now = clock_.now_nanoseconds();
    if (reopen_at_ != 0u && now < reopen_at_) return Status::no_space;
    const uint64_t size = next_block_bytes(bytes);
    if (size > largest_block_bytes || total_bytes_ + size > ceiling_bytes) return Status::no_space;
    uint32_t free_slot = 0u;
    while (free_slot < slot_count && blocks_[free_slot].constructed()) free_slot++;
    if (free_slot == slot_count) return Status::no_space;
    HostAllocation *made = nullptr;
    const Status    opened =
        channel_.open_memory(size, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), owner_, made);
    if (opened != Status::ok) {
        reopen_at_ = now + reopen_pause_nanoseconds;
        return opened;
    }
    const uint64_t usable = made->bytes() < slot_bytes ? made->bytes() : slot_bytes;
    const Status   built = blocks_[free_slot].construct(memory_, *made, usable, large_alignment);
    if (built != Status::ok) {
        release_memory(*made);
        return built;
    }
    total_bytes_ += blocks_[free_slot].object().bytes();
    slot = free_slot;
    return Status::ok;
}

void SliceBlocks::took(uint64_t offset)
{
    AV_SCOPE("SliceBlocks::took");
    blocks_[slot_of(offset)].object().took();
}

void SliceBlocks::remove(uint64_t offset)
{
    AV_SCOPE("SliceBlocks::remove");
    const uint32_t slot = slot_of(offset);
    if (holds(slot)) blocks_[slot].object().remove(within(offset));
}

void SliceBlocks::freed(uint64_t offset)
{
    AV_SCOPE("SliceBlocks::freed");
    const uint32_t slot = slot_of(offset);
    if (!holds(slot)) return;
    SliceBlock &block = blocks_[slot].object();
    block.freed();
    if (block.live() == 0u && !block.empty()) block.empty_since(clock_.now_nanoseconds());
}

uint32_t SliceBlocks::newest_empty() const
{
    AV_SCOPE("SliceBlocks::newest_empty");
    uint32_t newest = none;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (!blocks_[slot].constructed() || !blocks_[slot].object().empty()) continue;
        if (newest == none || blocks_[slot].object().emptied_at() >= blocks_[newest].object().emptied_at())
            newest = slot;
    }
    return newest;
}

bool SliceBlocks::lifetime_over(uint32_t slot, uint32_t spare, uint64_t now) const
{
    AV_SCOPE("SliceBlocks::lifetime_over");
    const uint64_t idle = now - blocks_[slot].object().emptied_at();
    return slot == spare ? idle >= spare_lifetime_nanoseconds : idle >= extra_lifetime_nanoseconds;
}

bool SliceBlocks::next_to_close(uint32_t &slot) const
{
    AV_SCOPE("SliceBlocks::next_to_close");
    const uint32_t spare = newest_empty();
    const uint64_t now = clock_.now_nanoseconds();
    for (uint32_t candidate = 0u; candidate < slot_count; candidate++) {
        if (!blocks_[candidate].constructed() || !blocks_[candidate].object().empty()) continue;
        if (!lifetime_over(candidate, spare, now)) continue;
        slot = candidate;
        return true;
    }
    return false;
}

void SliceBlocks::close(uint32_t slot)
{
    AV_SCOPE("SliceBlocks::close");
    if (!holds(slot)) return;
    HostAllocation *const allocation = blocks_[slot].object().allocation();
    total_bytes_ -= blocks_[slot].object().bytes();
    blocks_[slot].destroy();
    release_memory(*allocation);
}

void SliceBlocks::release_memory(HostAllocation &allocation)
{
    AV_SCOPE("SliceBlocks::release_memory");
    if (channel_.close_memory(allocation) == Status::ok || close_reported_) return;
    close_reported_ = true;
    lines_.write("slice memory could not be given back to the host");
}

bool SliceBlocks::close_unused(uint32_t slot)
{
    AV_SCOPE("SliceBlocks::close_unused");
    if (!holds(slot) || blocks_[slot].object().live() != 0u) return false;
    close(slot);
    return true;
}

uint32_t SliceBlocks::slot_of(uint64_t offset) const
{
    AV_SCOPE("SliceBlocks::slot_of");
    const uint64_t slot = offset >> slot_shift;
    return slot < slot_count ? static_cast<uint32_t>(slot) : slot_count;
}

uint64_t SliceBlocks::within(uint64_t offset) const
{
    AV_SCOPE("SliceBlocks::within");
    return offset & (slot_bytes - 1u);
}

bool SliceBlocks::holds(uint32_t slot) const
{
    AV_SCOPE("SliceBlocks::holds");
    return slot < slot_count && blocks_[slot].constructed();
}

uint64_t SliceBlocks::bytes_of(uint32_t slot) const
{
    AV_SCOPE("SliceBlocks::bytes_of");
    return holds(slot) ? blocks_[slot].object().bytes() : 0u;
}

uint8_t *SliceBlocks::view(uint64_t offset) const
{
    AV_SCOPE("SliceBlocks::view");
    return blocks_[slot_of(offset)].object().view() + within(offset);
}

uint64_t SliceBlocks::handle(uint64_t offset) const
{
    AV_SCOPE("SliceBlocks::handle");
    return blocks_[slot_of(offset)].object().handle();
}

uint32_t SliceBlocks::open_count() const
{
    AV_SCOPE("SliceBlocks::open_count");
    uint32_t opened = 0u;
    for (const ObjectStorage<SliceBlock> &block : blocks_) opened += block.constructed() ? 1u : 0u;
    return opened;
}

uint64_t SliceBlocks::open_bytes() const
{
    AV_SCOPE("SliceBlocks::open_bytes");
    return total_bytes_;
}

bool SliceBlocks::invariants_hold() const
{
    AV_SCOPE("SliceBlocks::invariants_hold");
    uint64_t total = 0u;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (!blocks_[slot].constructed()) continue;
        const SliceBlock &block = blocks_[slot].object();
        if (!block.invariants_hold() || block.bytes() > slot_bytes) return false;
        total += block.bytes();
    }
    return total == total_bytes_ && total <= ceiling_bytes;
}

}
