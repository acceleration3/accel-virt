#include "core/virtio/gpu_command_queue.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/virtio/virtio_pci_transport.hpp"
#include "core/virtio/virtq_segment.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GpuCommandQueue::GpuCommandQueue(Platform &platform, VirtioPciTransport &transport, uint16_t index,
                                 uint32_t response_bytes, LockRank rank)
    : platform_(platform), transport_(transport), queue_(platform, index), lock_(platform, rank),
      response_bytes_(response_bytes), slots_(nullptr), slots_address_(0u), busy_{}, slot_of_head_{},
      in_flight_(0u) {}

GpuCommandQueue::~GpuCommandQueue() { release_slots(); }

size_t GpuCommandQueue::slot_bytes() const { return GpuRequestWriter::max_request_size + response_bytes_; }

void GpuCommandQueue::release_slots()
{
    if (slots_ == nullptr) return;
    platform_.release_contiguous(slots_, slot_bytes() * slot_count);
    slots_ = nullptr;
}

Status GpuCommandQueue::activate(uint16_t vector)
{
    platform_.require_irql_at_most(Irql::passive, "GpuCommandQueue::activate");
    if (slots_ != nullptr) return Status::invalid_state;
    slots_ = static_cast<uint8_t *>(platform_.allocate_contiguous(slot_bytes() * slot_count));
    if (slots_ == nullptr) return Status::no_memory;
    slots_address_ = platform_.physical_address(slots_);
    const Status status = transport_.activate_queue(queue_, vector);
    if (status != Status::ok) release_slots();
    return status;
}

Status GpuCommandQueue::send(const uint8_t *request, size_t length)
{
    platform_.require_irql_at_most(Irql::dispatch, "GpuCommandQueue::send");
    if (slots_ == nullptr) return Status::invalid_state;
    if (request == nullptr || length == 0u || length > GpuRequestWriter::max_request_size)
        return Status::invalid_parameter;
    {
        SpinLockGuard lock(lock_);
        InvariantScope<GpuCommandQueue> guard(platform_, *this, "send");
        uint32_t slot = 0u;
        while (slot < slot_count && busy_[slot]) slot++;
        if (slot == slot_count) return Status::no_space;
        uint8_t *const bytes = slots_ + slot * slot_bytes();
        platform_.copy(bytes, request, length);
        const uint64_t address = slots_address_ + slot * slot_bytes();
        const VirtqSegment segments[] = {
            VirtqSegment(address, static_cast<uint32_t>(length), SegmentDirection::device_reads),
            VirtqSegment(address + GpuRequestWriter::max_request_size, response_bytes_, SegmentDirection::device_writes),
        };
        uint16_t head = 0u;
        const Status status = queue_.add(segments, response_bytes_ != 0u ? 2u : 1u, head);
        if (status != Status::ok) return status;
        busy_[slot] = true;
        slot_of_head_[head] = static_cast<uint8_t>(slot);
        in_flight_++;
    }
    transport_.notify(queue_);
    return Status::ok;
}

/* take_used frees a chain only after proving the queue published it, so its
 * head names a busy slot. */
Status GpuCommandQueue::harvest(uint32_t &completed, uint32_t &refused)
{
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
        const uint32_t type =
            WireReader(slots_ + slot * slot_bytes() + GpuRequestWriter::max_request_size, written).read_u32();
        if (written < GpuRequestWriter::response_size || type != GpuRequestWriter::response_ok_nodata) refused++;
    }
}

uint32_t GpuCommandQueue::in_flight() const
{
    platform_.require_irql_at_most(Irql::dispatch, "GpuCommandQueue::in_flight");
    return in_flight_;
}

bool GpuCommandQueue::invariants_hold() const
{
    uint32_t busy = 0u;
    for (bool slot : busy_) busy += slot ? 1u : 0u;
    return busy == in_flight_;
}

}  // namespace av
