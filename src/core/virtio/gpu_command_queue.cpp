#include "core/virtio/gpu_command_queue.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_pci_transport.hpp"
#include "core/virtio/virtq_segment.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GpuCommandQueue::GpuCommandQueue(Platform &platform, VirtioPciTransport &transport, uint16_t index,
                                 uint32_t response_bytes, uint32_t wide_bytes, LockRank rank)
    : platform_(platform), transport_(transport), queue_(platform, index), lock_(platform, rank),
      response_bytes_(response_bytes), slots_(nullptr), slots_address_(0u), wide_bytes_(wide_bytes),
      awaited_slot_(no_slot), awaited_response_(nullptr), awaited_capacity_(0u), awaited_written_(0u), busy_{},
      slot_of_head_{}, in_flight_(0u)
      {
          AV_SCOPE("GpuCommandQueue::GpuCommandQueue");
      }

GpuCommandQueue::~GpuCommandQueue()
{
    AV_SCOPE("GpuCommandQueue::~GpuCommandQueue");
    release_slots();
}

size_t GpuCommandQueue::slot_bytes() const
{
    AV_SCOPE("GpuCommandQueue::slot_bytes");
    return GpuRequestWriter::max_request_size + response_bytes_;
}

size_t GpuCommandQueue::slots_bytes() const
{
    AV_SCOPE("GpuCommandQueue::slots_bytes");
    return slot_bytes() * slot_count + (wide_bytes_ != 0u ? wide_bytes_ + response_bytes_ : 0u);
}

size_t GpuCommandQueue::offset_of(uint32_t slot) const
{
    AV_SCOPE("GpuCommandQueue::offset_of");
    return slot < slot_count ? slot * slot_bytes() : slot_count * slot_bytes();
}

size_t GpuCommandQueue::request_bytes_of(uint32_t slot) const
{
    AV_SCOPE("GpuCommandQueue::request_bytes_of");
    return slot < slot_count ? GpuRequestWriter::max_request_size : wide_bytes_;
}

uint32_t GpuCommandQueue::slot_for(size_t length) const
{
    AV_SCOPE("GpuCommandQueue::slot_for");
    /* send has already refused a request wider than the wide slot. */
    if (length > GpuRequestWriter::max_request_size) return busy_[wide_slot] ? no_slot : wide_slot;
    /* A narrow request never takes the wide slot: the backing of a primary
     * must not wait behind a burst of small ones. */
    uint32_t slot = 0u;
    while (slot < slot_count && busy_[slot]) slot++;
    return slot < slot_count ? slot : no_slot;
}

void GpuCommandQueue::release_slots()
{
    AV_SCOPE("GpuCommandQueue::release_slots");
    if (slots_ == nullptr) return;
    platform_.release_contiguous(slots_, slots_bytes());
    slots_ = nullptr;
}

Status GpuCommandQueue::activate(uint16_t vector)
{
    AV_SCOPE("GpuCommandQueue::activate");
    platform_.require_irql_at_most(Irql::passive, "GpuCommandQueue::activate");
    if (slots_ != nullptr) return Status::invalid_state;
    slots_ = static_cast<uint8_t *>(platform_.allocate_contiguous(slots_bytes()));
    if (slots_ == nullptr) return Status::no_memory;
    slots_address_ = platform_.physical_address(slots_);
    const Status status = transport_.activate_queue(queue_, vector);
    if (status != Status::ok) release_slots();
    return status;
}

Status GpuCommandQueue::send_locked(const uint8_t *request, size_t length, uint32_t &slot)
{
    AV_SCOPE("GpuCommandQueue::send_locked");
    InvariantScope<GpuCommandQueue> guard(platform_, *this, "send");
    slot = slot_for(length);
    if (slot == no_slot) return Status::no_space;
    const size_t   offset = offset_of(slot);
    uint8_t *const bytes = slots_ + offset;
    platform_.copy(bytes, request, length);
    const uint64_t address = slots_address_ + offset;
    const VirtqSegment segments[] = {
        VirtqSegment(address, static_cast<uint32_t>(length), SegmentDirection::device_reads),
        VirtqSegment(address + request_bytes_of(slot), response_bytes_, SegmentDirection::device_writes),
    };
    uint16_t head = 0u;
    const Status status = queue_.add(segments, response_bytes_ != 0u ? 2u : 1u, head);
    if (status != Status::ok) return status;
    busy_[slot] = true;
    slot_of_head_[head] = static_cast<uint8_t>(slot);
    in_flight_++;
    return Status::ok;
}

Status GpuCommandQueue::send(const uint8_t *request, size_t length)
{
    AV_SCOPE("GpuCommandQueue::send");
    platform_.require_irql_at_most(Irql::dispatch, "GpuCommandQueue::send");
    if (slots_ == nullptr) return Status::invalid_state;
    if (request == nullptr || length == 0u) return Status::invalid_parameter;
    if (length > GpuRequestWriter::max_request_size && length > wide_bytes_) return Status::too_large;
    {
        SpinLockGuard lock(lock_);
        uint32_t     slot = no_slot;
        const Status status = send_locked(request, length, slot);
        if (status != Status::ok) return status;
    }
    transport_.notify(queue_);
    return Status::ok;
}

/* The answer a request carries is only read here: the DPC's harvest counts and
 * drops the rest, so an exchange records the slot it is waiting on and takes
 * the bytes out of whichever caller harvests it first. */
Status GpuCommandQueue::exchange(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                 uint32_t &written)
{
    AV_SCOPE("GpuCommandQueue::exchange");
    platform_.require_irql_at_most(Irql::passive, "GpuCommandQueue::exchange");
    written = 0u;
    if (slots_ == nullptr) return Status::invalid_state;
    if (request == nullptr || length == 0u || response == nullptr || capacity == 0u || response_bytes_ == 0u)
        return Status::invalid_parameter;
    if (length > GpuRequestWriter::max_request_size && length > wide_bytes_) return Status::too_large;
    {
        SpinLockGuard lock(lock_);
        if (awaited_slot_ != no_slot) return Status::conflict;
        uint32_t     slot = no_slot;
        const Status status = send_locked(request, length, slot);
        if (status != Status::ok) return status;
        awaited_slot_ = slot;
        awaited_response_ = response;
        awaited_capacity_ = capacity;
        awaited_written_ = 0u;
    }
    transport_.notify(queue_);
    for (uint32_t poll = 0u; poll < polls; poll++) {
        uint32_t     completed = 0u;
        uint32_t     refused = 0u;
        const Status harvested = harvest(completed, refused);
        bool         answered = false;
        {
            /* The lock is dropped before the stall: a spin lock raises to
             * DISPATCH, and this path waits at PASSIVE. */
            SpinLockGuard lock(lock_);
            if (harvested != Status::ok) awaited_slot_ = no_slot;
            answered = awaited_slot_ == no_slot;
            written = awaited_written_;
        }
        if (harvested != Status::ok) return harvested;
        if (answered) return Status::ok;
        platform_.stall(poll_microseconds);
    }
    SpinLockGuard lock(lock_);
    awaited_slot_ = no_slot;
    written = 0u;
    return Status::not_found;
}

/* take_used frees a chain only after proving the queue published it, so its
 * head names a busy slot. */
Status GpuCommandQueue::harvest(uint32_t &completed, uint32_t &refused)
{
    AV_SCOPE("GpuCommandQueue::harvest");
    platform_.require_irql_at_most(Irql::dispatch, "GpuCommandQueue::harvest");
    completed = 0u;
    refused = 0u;
    if (slots_ == nullptr) return Status::invalid_state;
    SpinLockGuard lock(lock_);
    InvariantScope<GpuCommandQueue> guard(platform_, *this, "harvest");
    for (;;) {
        uint16_t head = 0u;
        uint32_t written = 0u;
        const Status status = queue_.take_used(head, written);
        if (status == Status::not_found) return Status::ok;
        if (status != Status::ok) return status;
        const uint32_t slot = slot_of_head_[head];
        busy_[slot] = false;
        in_flight_--;
        completed++;
        if (response_bytes_ == 0u) continue;
        const uint8_t *const answer = slots_ + offset_of(slot) + request_bytes_of(slot);
        if (slot == awaited_slot_) {
            const uint32_t taken = written < awaited_capacity_ ? written : static_cast<uint32_t>(awaited_capacity_);
            platform_.copy(awaited_response_, answer, taken);
            awaited_written_ = taken;
            awaited_slot_ = no_slot;
        }
        const uint32_t type = WireReader(answer, written).read_u32();
        if (written < GpuRequestWriter::response_size || type < GpuRequestWriter::response_ok_first ||
            type >= GpuRequestWriter::response_error_first)
            refused++;
    }
}

uint32_t GpuCommandQueue::in_flight() const
{
    AV_SCOPE("GpuCommandQueue::in_flight");
    platform_.require_irql_at_most(Irql::dispatch, "GpuCommandQueue::in_flight");
    return in_flight_;
}

bool GpuCommandQueue::invariants_hold() const
{
    AV_SCOPE("GpuCommandQueue::invariants_hold");
    uint32_t busy = 0u;
    for (bool slot : busy_) busy += slot ? 1u : 0u;
    return busy == in_flight_ && (awaited_slot_ == no_slot || busy_[awaited_slot_]);
}

}  // namespace av
