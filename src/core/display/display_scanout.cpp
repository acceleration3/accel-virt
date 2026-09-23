#include "core/display/display_scanout.hpp"

#include "core/display/display_mode.hpp"
#include "core/memory/allocation.hpp"
#include "proto/context/capset_info.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* One resource per output the driver can offer, each with its own index: an
 * array member cannot be built in a loop, so the maximum is written out. */
static_assert(DisplaySources::max_sources == 4u, "give each source its resource when there are more");

}  // namespace

DisplayScanout::DisplayScanout(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device)
    : platform_(platform), resources_{ScanoutResource(platform, budget, device, 0u),
                                      ScanoutResource(platform, budget, device, 1u),
                                      ScanoutResource(platform, budget, device, 2u),
                                      ScanoutResource(platform, budget, device, 3u)},
      blobs_{ScanoutBlobSource(platform, budget, device, 0u),
             ScanoutBlobSource(platform, budget, device, 1u),
             ScanoutBlobSource(platform, budget, device, 2u),
             ScanoutBlobSource(platform, budget, device, 3u)},
      blob_scanout_(false)
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
}

bool DisplayScanout::scans_out_blobs() const
{
    AV_SCOPE("DisplayScanout::scans_out_blobs");
    return blob_scanout_;
}

Status DisplayScanout::configure(uint32_t source, const DisplayMode &mode)
{
    AV_SCOPE("DisplayScanout::configure");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::configure");
    InvariantScope<DisplayScanout> guard(platform_, *this, "configure");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
    if (blob_scanout_) return blobs_[source].configure(mode);
    return resources_[source].configure(mode);
}

Status DisplayScanout::disable(uint32_t source)
{
    AV_SCOPE("DisplayScanout::disable");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::disable");
    InvariantScope<DisplayScanout> guard(platform_, *this, "disable");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
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
Status DisplayScanout::refresh(DisplaySources &sources, bool presented)
{
    AV_SCOPE("DisplayScanout::refresh");
    platform_.require_irql_at_most(Irql::dispatch, "DisplayScanout::refresh");
    InvariantScope<DisplayScanout> guard(platform_, *this, "refresh");
    Status carried = Status::ok;
    for (uint32_t source = 0u; source < sources.count(); source++) {
        if (!sources.visible(source)) continue;
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
            if (primary != nullptr) pages = primary->pages_at(address, resources_[source].page_count());
            if (primary != nullptr) key = primary->descriptor().host_key();
        }
        carried = resources_[source].refresh(address, pages, presented, key);
    }
    return carried;
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
