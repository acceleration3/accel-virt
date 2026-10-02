#include "miniport/present/present_copier.hpp"

#include "core/platform/platform.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "miniport/present/present_surface.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "miniport/virtio/virtq_segment.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentCopier::PresentCopier(Platform &platform, VirtioGpuDevice &device, PixelMover &fallback)
    : platform_(platform), device_(device), fallback_(fallback), payloads_(platform, LockRank::present_payload),
      payload_(nullptr), payload_address_(0u), refusal_logged_(false)
{
    AV_SCOPE("PresentCopier::PresentCopier");
}

PresentCopier::~PresentCopier()
{
    AV_SCOPE("PresentCopier::~PresentCopier");
    if (payload_ != nullptr) platform_.release_contiguous(payload_, payload_bytes);
}

Status PresentCopier::initialize()
{
    AV_SCOPE("PresentCopier::initialize");
    platform_.require_irql_at_most(Irql::passive, "PresentCopier::initialize");
    if (payload_ != nullptr) return Status::invalid_state;
    payload_ = static_cast<uint8_t *>(platform_.allocate_contiguous(payload_bytes));
    if (payload_ == nullptr) return Status::no_memory;
    payload_address_ = platform_.physical_address(payload_);
    return Status::ok;
}

Status PresentCopier::copy(const PresentSurface &source, const PresentSurface &destination,
                           const PresentRegion *regions, uint32_t count)
{
    AV_SCOPE("PresentCopier::copy");
    if (ask(source.blt_into(destination, count), &source, destination, regions, count) == Status::ok)
        return Status::ok;
    log_refusal();
    return fallback_.copy(source, destination, regions, count);
}

Status PresentCopier::fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                           uint32_t count)
{
    AV_SCOPE("PresentCopier::fill");
    if (ask(destination.fill_with(colour, count), nullptr, destination, regions, count) == Status::ok)
        return Status::ok;
    log_refusal();
    return fallback_.fill(destination, colour, regions, count);
}

Status PresentCopier::ask(const PresentCopyRequest &request, const PresentSurface *source,
                          const PresentSurface &destination, const PresentRegion *regions, uint32_t count)
{
    AV_SCOPE("PresentCopier::ask");
    platform_.require_irql_at_most(Irql::passive, "PresentCopier::ask");
    if (payload_ == nullptr) return Status::invalid_state;
    WaitMutexGuard one(payloads_);
    WireWriter     payload(payload_, payload_bytes);
    if (source != nullptr) source->write_runs(payload);
    destination.write_runs(payload);
    for (uint32_t index = 0u; index < count; index++) regions[index].write(payload);
    if (payload.status() != Status::ok) return payload.status();
    uint8_t    fixed[PresentCopyRequest::encoded_size] = {};
    WireWriter writer(fixed, sizeof fixed);
    request.write(writer);
    uint8_t      answer[answer_bytes] = {};
    uint32_t     answered = 0u;
    const Status asked = device_.ask_control_carrying(
        fixed, sizeof fixed,
        VirtqSegment(payload_address_, static_cast<uint32_t>(payload.offset()), SegmentDirection::device_reads),
        answer, sizeof answer, answered);
    if (asked != Status::ok) return asked;
    GpuResponseReader reader;
    const Status      decoded = reader.decode(answer, answered);
    if (decoded != Status::ok) return decoded;
    return reader.ok() ? Status::ok : Status::not_supported;
}

void PresentCopier::log_refusal()
{
    AV_SCOPE("PresentCopier::log_refusal");
    if (refusal_logged_) return;
    refusal_logged_ = true;
    platform_.log("avgpu: the host would not move a present's pixels; the miniport moves them itself");
}

}  // namespace av
