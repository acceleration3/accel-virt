#include "core/display/display_scanout.hpp"

#include "core/display/display_mode.hpp"
#include "core/memory/allocation.hpp"
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
                                      ScanoutResource(platform, budget, device, 3u)}
                                      {
                                          AV_SCOPE("DisplayScanout::DisplayScanout");
                                      }

Status DisplayScanout::configure(uint32_t source, const DisplayMode &mode)
{
    AV_SCOPE("DisplayScanout::configure");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::configure");
    InvariantScope<DisplayScanout> guard(platform_, *this, "configure");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
    return resources_[source].configure(mode);
}

Status DisplayScanout::disable(uint32_t source)
{
    AV_SCOPE("DisplayScanout::disable");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::disable");
    InvariantScope<DisplayScanout> guard(platform_, *this, "disable");
    if (source >= DisplaySources::max_sources) return Status::invalid_parameter;
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
        if (resources_[source].needs_pages(address)) {
            auto *const primary = static_cast<Allocation *>(sources.primary(source));
            if (primary != nullptr) pages = primary->pages_at(address, resources_[source].page_count());
        }
        carried = resources_[source].refresh(address, pages, presented);
    }
    return carried;
}

void DisplayScanout::release()
{
    AV_SCOPE("DisplayScanout::release");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::release");
    InvariantScope<DisplayScanout> guard(platform_, *this, "release");
    for (ScanoutResource &resource : resources_) resource.release();
}

void DisplayScanout::forget_device()
{
    AV_SCOPE("DisplayScanout::forget_device");
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::forget_device");
    for (ScanoutResource &resource : resources_) resource.forget_device();
}

bool DisplayScanout::invariants_hold() const
{
    AV_SCOPE("DisplayScanout::invariants_hold");
    for (const ScanoutResource &resource : resources_)
        if (!resource.invariants_hold()) return false;
    return true;
}

}  // namespace av
