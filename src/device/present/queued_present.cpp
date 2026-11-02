#include "device/present/queued_present.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/present/region_mover.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/guest_ram_lending.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "device/memory/lent_run.hpp"
#include "device/present/present_frames.hpp"
#include "core/support/scope_exit.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

static_assert(QueuedPresentRequest::max_entries <= GuestPageMapping::max_entries,
              "every page list a request may carry is one the device maps");
static_assert(QueuedPresentRequest::max_regions <= AV_FRAME_COPY_MOST_REGIONS,
              "every region list a request may carry is one the executor copies");
static_assert(QueuedPresentRequest::entry_size == GuestPageMapping::entry_size, "one memory entry, one layout");

QueuedPresent::QueuedPresent(Host &host)
    : host_(host), source_(host), destination_(host), present_(), regions_(), refusals_after_(0u), lent_fd_(-1),
      lending_(0u), lending_size_(0u), lent_offset_(0u), lendings_(nullptr), life_(nullptr), oldest_live_(0u),
      fallback_(Fallback::none)
{
    AV_SCOPE("QueuedPresent::QueuedPresent");
}

QueuedPresent::~QueuedPresent()
{
    AV_SCOPE("QueuedPresent::~QueuedPresent");
    forget_lending();
}

Status QueuedPresent::accept(ControlRequest &request, const QueuedPresentRequest &present,
                             const PresentFrames &snapshots, GuestRamLendings &lendings)
{
    AV_SCOPE("QueuedPresent::accept");
    if (present.source() == QueuedPresentRequest::Source::image && !snapshots.readable()) return Status::not_supported;
    present_ = present;
    Status status = map(request);
    if (status == Status::ok) status = read_regions(request);
    if (status != Status::ok) release();
    else if (present_.source() == QueuedPresentRequest::Source::image) lend(request, lendings);
    return status;
}

/* The pages are mapped already, so a present the GPU cannot take is moved
 * through them as before. A swap is the CPU's: the copy puts bytes where they
 * are. */
void QueuedPresent::lend(ControlRequest &request, GuestRamLendings &lendings)
{
    AV_SCOPE("QueuedPresent::lend");
    const PresentCopyRequest &copy = present_.copy();
    if (copy.swaps_red_and_blue()) {
        fallback_ = Fallback::swapped_colours;
        return;
    }
    LentRun      run;
    const Status found =
        lendings.locate(request, present_.destination_entries_offset(), copy.destination_entries(), run);
    if (found != Status::ok) {
        fallback_ = found == Status::not_found ? Fallback::not_lent : Fallback::scattered;
        return;
    }
    lent_fd_ = host_.duplicate_export(run.lending()->descriptor());
    if (lent_fd_ < 0) {
        fallback_ = Fallback::no_descriptor;
        return;
    }
    lending_ = run.lending()->id();
    lending_size_ = run.lending()->size();
    lent_offset_ = run.offset();
    lendings_ = &lendings;
    life_ = lendings.watch(*run.lending());
    oldest_live_ = lendings.oldest_live();
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
    if (present_.source() == QueuedPresentRequest::Source::image) {
        /* Given back before the move: the descriptor is of a dma-buf the guest
         * no longer has. One given back during it cannot be: the RAM is not
         * restored while the engine is mid-move (RunSwapGate), and the restore
         * carries what the copy wrote. */
        if (lent_fd_ >= 0 && life_->given_back()) fallback_ = Fallback::lending_gone;
        else if (lent_fd_ >= 0) {
            const Status copied = copy_snapshot(snapshots);
            if (copied != Status::not_supported) return copied;
        }
        return move_snapshot(snapshots);
    }
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

/* A row the buffer layout places is `pitch` bytes after the one before, in
 * four-byte texels. Nothing is multiplied before it is known to fit: the pitch
 * is a positive int, the rows the destination holds are counted by division,
 * and a region is inside them before its offset is formed - as RegionMover
 * does for its rows. A region whose row would run into the next has no layout
 * either, and the CPU moves it. */
bool QueuedPresent::place_regions(AvFrameRegion *out) const
{
    AV_SCOPE("QueuedPresent::place_regions");
    const uint32_t pitch = present_.copy().destination_pitch();
    if (pitch == 0u || pitch > static_cast<uint32_t>(INT32_MAX) || pitch % PresentRegion::pixel_bytes != 0u)
        return false;
    const uint64_t rows = destination_.length() / pitch;
    for (uint32_t index = 0u; index < present_.copy().regions(); index++) {
        const PresentRegion &region = regions_[index];
        const uint64_t       row_end = (uint64_t{region.destination_x()} + region.width()) * PresentRegion::pixel_bytes;
        if (row_end > pitch || region.destination_y() >= rows || region.height() > rows - region.destination_y())
            return false;
        out[index] = AvFrameRegion{region.source_x(), region.source_y(), region.destination_x(),
                                   region.destination_y(), region.width(), region.height()};
    }
    return true;
}

Status QueuedPresent::copy_snapshot(const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::copy_snapshot");
    AvFrameRegion placed[QueuedPresentRequest::max_regions];
    if (!place_regions(placed)) {
        fallback_ = Fallback::unplaceable;
        return Status::not_supported;
    }
    const AvFrameCopy copy = {lent_fd_, present_.copy().destination_pitch(), lending_, lending_size_, lent_offset_,
                              placed, present_.copy().regions(), 0u, oldest_live_};
    const Status      copied = snapshots.copy(present_.image(), copy);
    if (copied == Status::not_supported) fallback_ = Fallback::executor_declined;
    else snapshots.unmap(present_.image());
    return copied;
}

/* The snapshot is one extent of the executor's memory, rows `row_pitch`
 * apart: the regions move out of it as out of a surface of that pitch. */
Status QueuedPresent::move_snapshot(const PresentFrames &snapshots)
{
    AV_SCOPE("QueuedPresent::move_snapshot");
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

void QueuedPresent::release()
{
    AV_SCOPE("QueuedPresent::release");
    source_.release();
    destination_.release();
    forget_lending();
    present_ = QueuedPresentRequest();
    refusals_after_ = 0u;
    fallback_ = Fallback::none;
}

void QueuedPresent::forget_lending()
{
    AV_SCOPE("QueuedPresent::forget_lending");
    if (lent_fd_ >= 0) host_.close_export(lent_fd_);
    if (life_ != nullptr) lendings_->unwatch(life_);
    life_ = nullptr;
    lendings_ = nullptr;
    oldest_live_ = 0u;
    lent_fd_ = -1;
    lending_ = 0u;
    lending_size_ = 0u;
    lent_offset_ = 0u;
}

QueuedPresent::Fallback QueuedPresent::fallback() const
{
    AV_SCOPE("QueuedPresent::fallback");
    return fallback_;
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
    if ((lent_fd_ >= 0) != (lending_ != 0u) || (lent_fd_ >= 0) != (life_ != nullptr)) return false;
    return mapped == (present_.ticket() != 0u) && (mapped || refusals_after_ == 0u);
}

}  // namespace av
