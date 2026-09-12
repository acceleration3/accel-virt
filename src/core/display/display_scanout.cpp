#include "core/display/display_scanout.hpp"

#include "core/display/display_mode.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

namespace {

/* One resource per source, each with the source's index; an array member cannot
 * be built in a loop, and the count is fixed. */
static_assert(DisplaySources::source_count == 1u, "give each source its resource when there are more");

}  // namespace

DisplayScanout::DisplayScanout(Platform &platform, ObjectStorage<VirtioGpuDevice> &device)
    : platform_(platform), resources_{ScanoutResource(platform, device, 0u)} {}

Status DisplayScanout::configure(uint32_t source, const DisplayMode &mode)
{
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::configure");
    InvariantScope<DisplayScanout> guard(platform_, *this, "configure");
    if (source >= DisplaySources::source_count) return Status::invalid_parameter;
    return resources_[source].configure(mode);
}

Status DisplayScanout::disable(uint32_t source)
{
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::disable");
    InvariantScope<DisplayScanout> guard(platform_, *this, "disable");
    if (source >= DisplaySources::source_count) return Status::invalid_parameter;
    return resources_[source].disable();
}

/* A source that is not visible is not carried: dxgkrnl has told us nothing is
 * on it, and the host was already told to scan out nothing. */
Status DisplayScanout::refresh(DisplaySources &sources, bool presented)
{
    platform_.require_irql_at_most(Irql::dispatch, "DisplayScanout::refresh");
    InvariantScope<DisplayScanout> guard(platform_, *this, "refresh");
    Status carried = Status::ok;
    for (uint32_t source = 0u; source < DisplaySources::source_count; source++) {
        if (!sources.visible(source)) continue;
        carried = resources_[source].refresh(sources.scanout_address(source), presented);
    }
    return carried;
}

void DisplayScanout::release()
{
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::release");
    InvariantScope<DisplayScanout> guard(platform_, *this, "release");
    for (ScanoutResource &resource : resources_) resource.release();
}

void DisplayScanout::forget_device()
{
    platform_.require_irql_at_most(Irql::passive, "DisplayScanout::forget_device");
    for (ScanoutResource &resource : resources_) resource.forget_device();
}

bool DisplayScanout::invariants_hold() const
{
    for (const ScanoutResource &resource : resources_)
        if (!resource.invariants_hold()) return false;
    return true;
}

}  // namespace av
