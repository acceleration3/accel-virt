#include "virt_queue.hpp"

#include "invariant_scope.hpp"
#include "virtq_segment.hpp"

namespace av {

namespace {

constexpr uint16_t flag_next  = 1u;
constexpr uint16_t flag_write = 2u;
constexpr size_t   descriptor_bytes = 16u;

}  // namespace

/* The driver's own record of one descriptor, kept out of device-visible memory. */
struct VirtQueue::Shadow {
    uint16_t next;
    uint16_t chain_length;   /* non-zero only for the head of a chain in flight */
    uint32_t writable;
};

VirtQueue::VirtQueue(Platform &platform, uint16_t index)
    : platform_(platform), index_(index), size_(0u), ring_(platform), shadow_(nullptr),
      free_head_(0u), free_count_(0u), avail_index_(0u), last_used_index_(0u), driver_offset_(0u),
      device_offset_(0u), notify_offset_(0u) {}

VirtQueue::~VirtQueue()
{
    if (shadow_ != nullptr) platform_.release(shadow_);
}

Status VirtQueue::initialize(uint16_t device_max_size)
{
    platform_.require_irql_at_most(Irql::passive, "VirtQueue::initialize");
    InvariantScope<VirtQueue> guard(platform_, *this, "initialize");
    if (device_max_size == 0u || (device_max_size & (device_max_size - 1u)) != 0u) return Status::malformed;
    if (size_ != 0u) return Status::invalid_state;
    const uint16_t size = device_max_size < max_size ? device_max_size : max_size;

    const size_t driver_offset = descriptor_bytes * size;
    const size_t device_offset = (driver_offset + 6u + 2u * size + 3u) & ~size_t{3};
    auto *shadow = static_cast<Shadow *>(platform_.allocate(sizeof(Shadow) * size, Pool::non_paged));
    if (shadow == nullptr) return Status::no_memory;
    const Status ring = ring_.initialize(device_offset + 6u + 8u * size);
    if (ring != Status::ok) {
        platform_.release(shadow);
        return ring;
    }
    for (uint16_t i = 0u; i < size; i++) shadow[i] = Shadow{static_cast<uint16_t>(i + 1u), 0u, 0u};
    shadow_ = shadow;
    size_ = size;
    free_count_ = size;
    driver_offset_ = driver_offset;
    device_offset_ = device_offset;
    return Status::ok;
}

void VirtQueue::bind_notify_offset(uint64_t offset) { notify_offset_ = offset; }

uint16_t VirtQueue::read_u16(size_t offset) const
{
    const uint8_t *bytes = ring_.bytes();
    return static_cast<uint16_t>(bytes[offset] | bytes[offset + 1u] << 8u);
}

uint32_t VirtQueue::read_u32(size_t offset) const
{
    return uint32_t{read_u16(offset)} | uint32_t{read_u16(offset + 2u)} << 16u;
}

void VirtQueue::write_u16(size_t offset, uint16_t value)
{
    ring_.bytes()[offset] = static_cast<uint8_t>(value);
    ring_.bytes()[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

void VirtQueue::write_u32(size_t offset, uint32_t value)
{
    write_u16(offset, static_cast<uint16_t>(value));
    write_u16(offset + 2u, static_cast<uint16_t>(value >> 16u));
}

void VirtQueue::write_u64(size_t offset, uint64_t value)
{
    write_u32(offset, static_cast<uint32_t>(value));
    write_u32(offset + 4u, static_cast<uint32_t>(value >> 32u));
}

Status VirtQueue::add(const VirtqSegment *segments, uint16_t count, uint16_t &head)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtQueue::add");
    InvariantScope<VirtQueue> guard(platform_, *this, "add");
    if (size_ == 0u) return Status::invalid_state;
    if (count == 0u) return Status::invalid_parameter;
    if (count > free_count_) return Status::no_space;
    uint64_t writable = 0u;
    for (uint16_t i = 0u; i < count; i++)
        if (segments[i].direction() == SegmentDirection::device_writes) writable += segments[i].length();
    if (writable > 0xFFFFFFFFu) return Status::invalid_parameter;

    const uint16_t first = free_head_;
    uint16_t descriptor = first;
    for (uint16_t i = 0u; i < count; i++) {
        const size_t at = descriptor_bytes * descriptor;
        const bool last = i + 1u == count;
        const uint16_t flags = static_cast<uint16_t>((last ? 0u : flag_next) |
            (segments[i].direction() == SegmentDirection::device_writes ? flag_write : 0u));
        write_u64(at, segments[i].physical_address());
        write_u32(at + 8u, segments[i].length());
        write_u16(at + 12u, flags);
        write_u16(at + 14u, last ? 0u : shadow_[descriptor].next);
        if (!last) descriptor = shadow_[descriptor].next;
    }
    free_head_ = shadow_[descriptor].next;
    free_count_ = static_cast<uint16_t>(free_count_ - count);
    shadow_[first].chain_length = count;
    shadow_[first].writable = static_cast<uint32_t>(writable);

    write_u16(driver_offset_ + 4u + 2u * (avail_index_ & (size_ - 1u)), first);
    avail_index_++;
    write_u16(driver_offset_ + 2u, avail_index_);
    head = first;
    return Status::ok;
}

void VirtQueue::free_chain(uint16_t head)
{
    uint16_t last = head;
    for (uint16_t i = 1u; i < shadow_[head].chain_length; i++) last = shadow_[last].next;
    shadow_[last].next = free_head_;
    free_head_ = head;
    free_count_ = static_cast<uint16_t>(free_count_ + shadow_[head].chain_length);
    shadow_[head].chain_length = 0u;
    shadow_[head].writable = 0u;
}

Status VirtQueue::take_used(uint16_t &head, uint32_t &written)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtQueue::take_used");
    InvariantScope<VirtQueue> guard(platform_, *this, "take_used");
    if (size_ == 0u) return Status::invalid_state;
    const uint16_t device_index = read_u16(device_offset_ + 2u);
    const uint16_t pending = static_cast<uint16_t>(device_index - last_used_index_);
    if (pending == 0u) return Status::not_found;
    if (pending > static_cast<uint16_t>(avail_index_ - last_used_index_)) return Status::invalid_state;

    const size_t element = device_offset_ + 4u + 8u * (last_used_index_ & (size_ - 1u));
    const uint32_t id = read_u32(element);
    const uint32_t length = read_u32(element + 4u);
    /* A bad element is consumed so the queue moves on; its chain, if any, is
     * lost rather than freed on the device's word. */
    last_used_index_++;
    if (id >= size_ || shadow_[id].chain_length == 0u || length > shadow_[id].writable)
        return Status::invalid_state;
    free_chain(static_cast<uint16_t>(id));
    head = static_cast<uint16_t>(id);
    written = length;
    return Status::ok;
}

uint16_t VirtQueue::index() const            { return index_; }
uint16_t VirtQueue::size() const             { return size_; }
uint16_t VirtQueue::free_descriptors() const { return free_count_; }
uint64_t VirtQueue::descriptor_area() const  { return ring_.physical_address(); }
uint64_t VirtQueue::driver_area() const      { return ring_.physical_address() + driver_offset_; }
uint64_t VirtQueue::device_area() const      { return ring_.physical_address() + device_offset_; }
uint64_t VirtQueue::notify_offset() const    { return notify_offset_; }

bool VirtQueue::invariants_hold() const
{
    const uint16_t in_flight = static_cast<uint16_t>(avail_index_ - last_used_index_);
    return free_count_ <= size_ && in_flight <= size_ && (size_ == 0u) == (shadow_ == nullptr);
}

}  // namespace av
