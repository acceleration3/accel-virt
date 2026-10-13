#include "device/present/queued_present.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/present/region_mover.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_dma_buf.hpp"
#include "device/memory/blob_window.hpp"
#include "device/present/frame_destination.hpp"
#include "device/present/present_frames.hpp"
#include "core/support/scope_exit.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

static_assert(QueuedPresentRequest::max_entries <= GuestPageMapping::max_entries,
              "every page list a request may carry is one the device maps");
static_assert(QueuedPresentRequest::entry_size == GuestPageMapping::entry_size, "one memory entry, one layout");

QueuedPresent::QueuedPresent(Host &host)
    : host_(host), source_(host), destination_(host), present_(), regions_(), refusals_after_(0u)
{
    AV_SCOPE("QueuedPresent::QueuedPresent");
}

Status QueuedPresent::accept(ControlRequest &request, const QueuedPresentRequest &present,
                             const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::accept");
    if (present.source() == QueuedPresentRequest::Source::image && !snapshots.readable()) return Status::not_supported;
    present_ = present;
    Status status = map(request);
    if (status == Status::ok) status = read_regions(request);
    if (status != Status::ok) release();
    return status;
}

Status QueuedPresent::map(ControlRequest &request)
{
    AV_SCOPE("QueuedPresent::map");
    const PresentCopyRequest &copy = present_.copy();
    if (copy.operation() == PresentOperation::blt && present_.source() == QueuedPresentRequest::Source::pages) {
        const Status mapped = source_.map(request, QueuedPresentRequest::encoded_size, copy.source_entries());
        if (mapped != Status::ok) return mapped;
    }
    return destination_.map(request, present_.destination_entries_offset(), copy.destination_entries());
}

Status QueuedPresent::read_regions(ControlRequest &request)
{
    AV_SCOPE("QueuedPresent::read_regions");
    for (uint32_t index = 0u; index < present_.copy().regions(); index++) {
        uint8_t    bytes[PresentRegion::encoded_size] = {};
        WireReader reader(bytes, request.read(present_.regions_offset() + index * PresentRegion::encoded_size, bytes,
                                              sizeof bytes));
        if (regions_[index].decode(reader) != Status::ok) return Status::invalid_parameter;
    }
    return Status::ok;
}

Status QueuedPresent::move(const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::move");
    if (present_.source() == QueuedPresentRequest::Source::image) return move_snapshot(snapshots);
    RegionMover mover(source_.window(), destination_.window(), present_.copy());
    return move_regions(mover);
}

Status QueuedPresent::move_regions(RegionMover &mover)
{
    AV_SCOPE("QueuedPresent::move_regions");
    for (uint32_t index = 0u; index < present_.copy().regions(); index++)
        if (mover.move(regions_[index]) != Status::ok) return Status::invalid_parameter;
    return Status::ok;
}

/* The snapshot is one extent of the executor's memory, rows `row_pitch`
 * apart: the regions move out of it as out of a surface of that pitch. */
Status QueuedPresent::move_snapshot(const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::move_snapshot");
    if (write_snapshot(snapshots)) return Status::ok;
    AvFrameView view = {};
    if (!snapshots.map(present_.image(), view)) return Status::not_found;
    ScopeExit       unmap([this, &snapshots] { snapshots.unmap(present_.image()); });
    void *const     pixels = const_cast<uint8_t *>(view.pixels);
    const uint64_t  length = uint64_t{view.row_pitch} * view.height;
    const BlobWindow source(&pixels, &length, 1u, length);
    const PresentCopyRequest &copy = present_.copy();
    const PresentCopyRequest  shape(PresentOperation::blt,
                                    copy.swaps_red_and_blue() ? PresentCopyRequest::flag_swap_red_blue : 0u, 0u,
                                    copy.regions(), 1u, view.row_pitch, copy.destination_entries(),
                                    copy.destination_pitch());
    RegionMover mover(source, destination_.window(), shape);
    return move_regions(mover);
}

/* The destination's pages as one dma-buf the GPU writes in place: no CPU reads
 * or writes a pixel of the frame (docs/compositor.md). Swapping red and blue
 * is a CPU's job, and so is any write the host turns down. */
bool QueuedPresent::write_snapshot(const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::write_snapshot");
    const PresentCopyRequest &copy = present_.copy();
    if (copy.swaps_red_and_blue()) return false;
    BlobDmaBuf pages(host_);
    if (pages.make(destination_.window(), 0u, destination_.window().length()) != Status::ok) return false;
    FrameDestination destination(pages.handle(), pages.length(), copy.destination_pitch());
    for (uint32_t index = 0u; index < copy.regions(); index++)
        if (!destination.add(regions_[index])) return false;
    return snapshots.write(present_.image(), destination);
}

void QueuedPresent::release()
{
    AV_SCOPE("QueuedPresent::release");
    source_.release();
    destination_.release();
    present_ = QueuedPresentRequest();
    refusals_after_ = 0u;
}

uint64_t QueuedPresent::ticket() const
{
    AV_SCOPE("QueuedPresent::ticket");
    return present_.ticket();
}

void QueuedPresent::follow_with_refusal()
{
    AV_SCOPE("QueuedPresent::follow_with_refusal");
    refusals_after_++;
}

uint64_t QueuedPresent::refusals_after() const
{
    AV_SCOPE("QueuedPresent::refusals_after");
    return refusals_after_;
}

bool QueuedPresent::invariants_hold() const
{
    AV_SCOPE("QueuedPresent::invariants_hold");
    if (!source_.invariants_hold() || !destination_.invariants_hold()) return false;
    const bool mapped = destination_.extents() != 0u;
    return mapped == (present_.ticket() != 0u) && (mapped || refusals_after_ == 0u);
}

}  // namespace av
