#include "core/display/synthetic_vsync.hpp"

#include "core/ddi/entry_points.hpp"
#include "core/display/display_sources.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"

namespace av {

SyntheticVsync::SyntheticVsync(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources)
    : platform_(platform), dxgkrnl_(dxgkrnl), sources_(sources), notification_(platform, dxgkrnl),
      timer_(platform, &AvVsyncTick, this) {}

Status SyntheticVsync::initialize()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::initialize");
    return timer_.initialize();
}

Status SyntheticVsync::enable()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::enable");
    return timer_.start(frame_period);
}

void SyntheticVsync::disable()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::disable");
    timer_.stop();
}

void SyntheticVsync::destroy()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::destroy");
    timer_.destroy();
}

void SyntheticVsync::tick()
{
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::tick");
    if (notification_.report_vsync(target, sources_.scanout_address(target)) != Status::ok) {
        platform_.log("rvkgpu: a vsync was refused");
        return;
    }
    dxgkrnl_.notify_dpc();
}

}  // namespace av
