#include "core/trace/event_ring.hpp"

namespace av {

namespace {

constexpr uint32_t begin_bit = 0x80000000u;

void put32(uint8_t *at, uint32_t value)
{
    __builtin_memcpy(at, &value, sizeof value);
}

void put64(uint8_t *at, uint64_t value)
{
    __builtin_memcpy(at, &value, sizeof value);
}

}  // namespace

EventRing::EventRing() : region_(nullptr), slots_(0u), events_per_slot_(0u) {}

size_t EventRing::bytes_for(uint32_t slots, uint32_t events_per_slot)
{
    return header_bytes + static_cast<size_t>(slots) * (slot_header_bytes + static_cast<size_t>(events_per_slot) * event_bytes);
}

Status EventRing::initialize(uint8_t *region, size_t bytes, uint32_t slots, uint32_t events_per_slot)
{
    if (region == nullptr || slots == 0u || events_per_slot == 0u || bytes < bytes_for(slots, events_per_slot))
        return Status::invalid_parameter;
    put32(region, magic);
    put32(region + 4u, version);
    put32(region + 8u, slots);
    put32(region + 12u, events_per_slot);
    region_ = region;
    slots_ = slots;
    events_per_slot_ = events_per_slot;
    return Status::ok;
}

void EventRing::describe_clock(uint64_t ticks_per_second)
{
    if (region_ != nullptr) put64(region_ + 16u, ticks_per_second);
}

bool EventRing::recording() const
{
    return region_ != nullptr;
}

void EventRing::record(uint32_t slot, uint64_t time, uint32_t name, uint32_t thread, bool begin)
{
    /* Header bytes 24..31 are a stop time a reader sets: events from then on
     * are not recorded, so a ring stops at the same instant as the others a
     * capture reads, and holds still while it is copied. 0 records; 1 - a
     * reader setting byte 24 alone - stops it now. */
    if (region_ == nullptr || slot >= slots_) return;
    const uint64_t stop = __atomic_load_n(reinterpret_cast<const uint64_t *>(region_ + 24u), __ATOMIC_RELAXED);
    if (stop != 0u && time >= stop) return;
    uint8_t *const base = region_ + header_bytes + static_cast<size_t>(slot) * (slot_header_bytes +
                                                                              static_cast<size_t>(events_per_slot_) * event_bytes);
    uint64_t written = 0u;
    __builtin_memcpy(&written, base, sizeof written);
    uint8_t *const event = base + slot_header_bytes + (written % events_per_slot_) * event_bytes;
    put64(event, time);
    put32(event + 8u, name);
    put32(event + 12u, (thread & ~begin_bit) | (begin ? begin_bit : 0u));
    /* The count moves after the event is whole, so a reader never takes a
     * half-written one as the newest. */
    __atomic_store_n(reinterpret_cast<uint64_t *>(base), written + 1u, __ATOMIC_RELEASE);
}

}  // namespace av
