#include "miniport/present/present_queue.hpp"

#include "core/platform/platform.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "miniport/present/present_surface.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/surface_point.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "miniport/virtio/virtq_segment.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/pages/present_progress.hpp"
#include "miniport/present/present_ticket_record.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr size_t   answer_bytes = 24u;
constexpr uint32_t poll_microseconds = 50u;

}

PresentQueue::PresentQueue(Platform &platform, VirtioGpuDevice &device, uint32_t epoch)
    : platform_(platform), device_(device), sending_(platform, LockRank::present_queue), page_(platform),
      payloads_(), epoch_(epoch), started_(false), queued_(0u)
{
    AV_SCOPE("PresentQueue::PresentQueue");
}

PresentQueue::~PresentQueue()
{
    AV_SCOPE("PresentQueue::~PresentQueue");
    for (uint8_t *payload : payloads_)
        if (payload != nullptr) platform_.release_contiguous(payload, payload_bytes);
}

Status PresentQueue::start()
{
    AV_SCOPE("PresentQueue::start");
    platform_.require_irql_at_most(Irql::passive, "PresentQueue::start");
    if (started_) return Status::invalid_state;
    Status status = page_.address() == 0u ? page_.initialize() : Status::ok;
    for (uint8_t *&payload : payloads_)
        if (status == Status::ok && payload == nullptr) {
            payload = static_cast<uint8_t *>(platform_.allocate_contiguous(payload_bytes));
            if (payload == nullptr) status = Status::no_memory;
        }
    if (status != Status::ok) return status;
    uint8_t          request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter writer(request, sizeof request);
    static_cast<void>(writer.present_engine(page_.address()));
    uint8_t      answer[answer_bytes] = {};
    uint32_t     written = 0u;
    const Status asked = device_.ask_control(request, writer.length(), answer, sizeof answer, written);
    if (asked != Status::ok) return asked;
    GpuResponseReader response;
    const Status      decoded = response.decode(answer, written);
    if (decoded != Status::ok) return decoded;
    if (!response.ok()) return Status::not_supported;
    started_ = true;
    return Status::ok;
}

Status PresentQueue::queue(const PresentCopyRequest &request, const PresentSurface *source,
                           const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket)
{
    AV_SCOPE("PresentQueue::queue");
    platform_.require_irql_at_most(Irql::passive, "PresentQueue::queue");
    return enqueue(SurfacePoint(), request, source, destination, regions, ticket);
}

Status PresentQueue::queue_image(const SurfacePoint &image, const PresentCopyRequest &request,
                                 const PresentSurface &destination,
                                 const PresentRegion *regions, uint64_t &ticket)
{
    AV_SCOPE("PresentQueue::queue_image");
    platform_.require_irql_at_most(Irql::passive, "PresentQueue::queue_image");
    const PresentCopyRequest from_image(PresentOperation::blt,
                                        request.swaps_red_and_blue() ? PresentCopyRequest::flag_swap_red_blue : 0u,
                                        0u, request.regions(), 0u, 0u, request.destination_entries(),
                                        request.destination_pitch());
    return enqueue(image, from_image, nullptr, destination, regions, ticket);
}

Status PresentQueue::enqueue(const SurfacePoint &image, const PresentCopyRequest &request, const PresentSurface *source,
                             const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket)
{
    AV_SCOPE("PresentQueue::enqueue");
    ticket = 0u;
    if (!started_) return Status::invalid_state;
    WaitMutexGuard one(sending_);
    const uint64_t next = queued_ + 1u;
    size_t         length = 0u;
    Status         status = wait_for_payload(next);
    if (status == Status::ok)
        status = write_payload(slot_of(next), source, destination, regions, request.regions(), length);
    const QueuedPresentRequest present = image.names() ? QueuedPresentRequest(next, image, request)
                                                       : QueuedPresentRequest(next, request);
    if (status == Status::ok) status = send(present, slot_of(next), length);
    if (status != Status::ok) return status;
    platform_.store_atomic(queued_, next);
    ticket = next;
    return Status::ok;
}

uint32_t PresentQueue::slot_of(uint64_t ticket) const
{
    AV_SCOPE("PresentQueue::slot_of");
    return static_cast<uint32_t>((ticket - 1u) % slots);
}

Status PresentQueue::wait_for_payload(uint64_t ticket)
{
    AV_SCOPE("PresentQueue::wait_for_payload");
    if (ticket <= slots) return Status::ok;
    for (uint32_t waited = 0u; completed() < ticket - slots; waited += poll_microseconds) {
        if (waited >= wait_microseconds) return Status::not_found;
        platform_.stall(poll_microseconds);
    }
    return Status::ok;
}

Status PresentQueue::write_payload(uint32_t slot, const PresentSurface *source, const PresentSurface &destination,
                                   const PresentRegion *regions, uint32_t count, size_t &length)
{
    AV_SCOPE("PresentQueue::write_payload");
    WireWriter payload(payloads_[slot], payload_bytes);
    if (source != nullptr) source->write_runs(payload);
    destination.write_runs(payload);
    for (uint32_t index = 0u; index < count; index++) regions[index].write(payload);
    if (payload.status() != Status::ok) return Status::too_large;
    length = payload.offset();
    return Status::ok;
}

Status PresentQueue::send(const QueuedPresentRequest &present, uint32_t slot, size_t length)
{
    AV_SCOPE("PresentQueue::send");
    uint8_t    fixed[QueuedPresentRequest::encoded_size] = {};
    WireWriter writer(fixed, sizeof fixed);
    present.write(writer);
    const VirtqSegment payload(platform_.physical_address(payloads_[slot]), static_cast<uint32_t>(length),
                               SegmentDirection::device_reads);
    for (uint32_t waited = 0u;; waited += poll_microseconds) {
        const Status sent = device_.send_control_carrying(fixed, sizeof fixed, payload);
        if (sent != Status::no_space) return sent;
        if (waited >= wait_microseconds) return Status::not_found;
        uint32_t harvested = 0u;
        static_cast<void>(device_.harvest_control(harvested));
        platform_.stall(poll_microseconds);
    }
}

uint64_t PresentQueue::completed() const
{
    AV_SCOPE("PresentQueue::completed");
    platform_.require_irql_at_most(Irql::dispatch, "PresentQueue::completed");
    PresentProgress progress;
    return page_.read(progress) == Status::ok ? progress.completed() : 0u;
}

bool PresentQueue::holds(const PresentTicketRecord &record)
{
    AV_SCOPE("PresentQueue::holds");
    platform_.require_irql_at_most(Irql::dispatch, "PresentQueue::holds");
    return record.epoch() == epoch_ && record.ticket() != 0u && record.ticket() <= platform_.load_atomic(queued_);
}

const PresentPageView &PresentQueue::progress() const
{
    AV_SCOPE("PresentQueue::progress");
    return page_;
}

uint32_t PresentQueue::epoch() const
{
    AV_SCOPE("PresentQueue::epoch");
    return epoch_;
}

bool PresentQueue::invariants_hold() const
{
    AV_SCOPE("PresentQueue::invariants_hold");
    if (!started_) return queued_ == 0u;
    if (page_.address() == 0u) return false;
    for (const uint8_t *payload : payloads_)
        if (payload == nullptr) return false;
    return true;
}

}
