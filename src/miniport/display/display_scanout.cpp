#include "miniport/display/display_scanout.hpp"

#include "miniport/virtio/virtio_gpu_device.hpp"

#include "core/support/text_buffer.hpp"

#include "miniport/display/display_mode.hpp"
#include "miniport/memory/allocation.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "proto/context/capset_info.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* One resource per output the driver can offer, each with its own index: an
 * array member cannot be built in a loop, so the maximum is written out. */
static_assert(DisplaySources::max_sources == 4u, "give each source its resource when there are more");

}  // namespace

DisplayScanout::DisplayScanout(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device)
    : platform_(platform), lock_(platform, LockRank::scanout), resources_{ScanoutResource(platform, budget, device, 0u),
                                      ScanoutResource(platform, budget, device, 1u),
                                      ScanoutResource(platform, budget, device, 2u),
                                      ScanoutResource(platform, budget, device, 3u)},
      blobs_{ScanoutBlobSource(platform, budget, device, 0u),
             ScanoutBlobSource(platform, budget, device, 1u),
             ScanoutBlobSource(platform, budget, device, 2u),
             ScanoutBlobSource(platform, budget, device, 3u)},
      blob_scanout_(false), planes_(device), planes_offered_(false)
      {
          AV_SCOPE("DisplayScanout::DisplayScanout");
      }

/* Read once, at StartDevice, beside every other thing the capset settles. A
 * device that cannot show a dma-buf is every device until its display backend
 * gives a console a GL context, so the 2D path stays the default. */
void DisplayScanout::offer(const CapsetInfo &capset)
{
    AV_SCOPE("DisplayScanout::offer");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::offer");
    blob_scanout_ = capset.has_feature(CapsetInfo::feature_dmabuf_scanout);
    planes_offered_ = capset.has_feature(CapsetInfo::feature_scanout_planes);
}

bool DisplayScanout::carries_planes() const
{
    AV_SCOPE("DisplayScanout::carries_planes");
    return planes_offered_;
}

bool DisplayScanout::scans_out_blobs() const
{
    AV_SCOPE("DisplayScanout::scans_out_blobs");
    return blob_scanout_;
}

/* The room the mode needs is made before the lock, where memory can be had;
 * the source takes it under the lock. */
Status DisplayScanout::configure(uint32_t source, const DisplayMode &mode)
{
    AV_SCOPE("DisplayScanout::configure");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::configure");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
    const Status room = blob_scanout_ ? blobs_[source].prepare(mode) : resources_[source].prepare(mode);
    if (room != Status::ok) return room;
    SpinLockGuard                  held(lock_);
    InvariantScope<DisplayScanout> guard(platform_, *this, "configure");
    if (blob_scanout_) return blobs_[source].configure(mode);
    return resources_[source].configure(mode);
}

Status DisplayScanout::disable(uint32_t source)
{
    AV_SCOPE("DisplayScanout::disable");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::disable");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
    SpinLockGuard                  held(lock_);
    InvariantScope<DisplayScanout> guard(platform_, *this, "disable");
    if (blob_scanout_) return blobs_[source].disable();
    return resources_[source].disable();
}

/* A source that is not visible is not carried: dxgkrnl has told us nothing is
 * on it, and the host was already told to scan out nothing.
 *
 * The primary handle is the one this driver gave dxgkrnl for its own
 * allocation, so it is cast back rather than looked up: the allocation table
 * is reached under a mutex, which this path may not take, and
 * DisplaySources::forget_primary clears the handle before the allocation is
 * destroyed. */
Status DisplayScanout::refresh(DisplaySources &sources, bool presented, bool due)
{
    AV_SCOPE("DisplayScanout::refresh");
    platform_.require_irql_at_most(Irql::dispatch, "DisplayScanout::refresh");
    SpinLockGuard                  held(lock_);
    InvariantScope<DisplayScanout> guard(platform_, *this, "refresh");
    Status                         carried = Status::ok;
    for (uint32_t source = 0u; source < sources.count(); source++) {
        if (!sources.visible(source) || (!due && !numbered(source))) continue;
        /* The number before the address: a flip in between then posts the new
         * address under the older number, and the next DPC posts it again
         * under its own, rather than the old address under the new one. */
        const uint32_t frame = sources.frame(source);
        const uint64_t address = sources.scanout_address(source);
        GuestPageList  pages;
        if (blob_scanout_) {
            if (blobs_[source].needs_pages(address)) {
                auto *const primary = static_cast<Allocation *>(sources.primary(source));
                if (primary != nullptr) pages = primary->pages_at(address, blobs_[source].page_count());
            }
            carried = blobs_[source].refresh(address, pages, presented);
            continue;
        }
        uint64_t key = 0u;
        if (resources_[source].needs_pages(address)) {
            auto *const primary = static_cast<Allocation *>(sources.primary(source));
            if (primary != nullptr) key = primary->host_key();
            if (key == 0u && resources_[source].keyed()) {
                char       bytes[160];
                TextBuffer line(bytes, sizeof bytes);
                line.append("avgpu: unkeyed scanout address 0x");
                line.append_hex(address, 16u);
                line.append(" primary 0x");
                line.append_hex(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(primary)), 16u);
                line.append(" kind ");
                line.append_decimal(primary != nullptr ? static_cast<uint32_t>(primary->descriptor().kind()) : 0u);
                line.append(" flip ");
                line.append_decimal(static_cast<uint32_t>(sources.flip(source)));
                platform_.log(line.text());
            }
            /* A keyed image already shown never reads the pages (ScanoutResource::refresh). */
            if (primary != nullptr && (key == 0u || !resources_[source].scanning_out()))
                pages = primary->pages_at(address, resources_[source].page_count());
        }
        /* The planes first, so the frame that follows is composed with them. A
         * source flipped some other way shows its base image alone. */
        if (planes_offered_ && resources_[source].width() != 0u) {
            OverlayPlane planes[OverlayPlane::max_planes];
            if (sources.flip(source) == DisplaySources::Flip::presented_plane) sources.planes(source, planes);
            const Status planed =
                planes_.carry(source, planes, resources_[source].width(), resources_[source].height());
            if (planed != Status::ok) carried = planed;
        }
        const Status refreshed = resources_[source].refresh(address, pages, presented, key, frame);
        if (refreshed != Status::ok) carried = refreshed;
    }
    return carried;
}

bool DisplayScanout::posts_frames(uint32_t source)
{
    AV_SCOPE("DisplayScanout::posts_frames");
    platform_.require_irql_at_most(Irql::dispatch, "DisplayScanout::posts_frames");
    SpinLockGuard held(lock_);
    return numbered(source);
}

bool DisplayScanout::numbered(uint32_t source) const
{
    AV_SCOPE("DisplayScanout::numbered");
    return !blob_scanout_ && source < DisplaySources::max_sources && resources_[source].posts_frames();
}

void DisplayScanout::release()
{
    AV_SCOPE("DisplayScanout::release");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::release");
    InvariantScope<DisplayScanout> guard(platform_, *this, "release");
    for (ScanoutResource &resource : resources_) resource.release();
    for (ScanoutBlobSource &blob : blobs_) blob.release();
}

void DisplayScanout::forget_device()
{
    AV_SCOPE("DisplayScanout::forget_device");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::forget_device");
    for (ScanoutResource &resource : resources_) resource.forget_device();
    for (ScanoutBlobSource &blob : blobs_) blob.forget_device();
    planes_.forget_device();
}

bool DisplayScanout::invariants_hold() const
{
    AV_SCOPE("DisplayScanout::invariants_hold");
    for (const ScanoutResource &resource : resources_)
        if (!resource.invariants_hold()) return false;
    for (const ScanoutBlobSource &blob : blobs_)
        if (!blob.invariants_hold()) return false;
    return true;
}

}  // namespace av
