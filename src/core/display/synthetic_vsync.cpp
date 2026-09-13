#include "core/display/synthetic_vsync.hpp"

#include "core/ddi/entry_points.hpp"
#include "core/display/display_sources.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/support/text_buffer.hpp"

namespace av {

SyntheticVsync::SyntheticVsync(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources)
    : platform_(platform), dxgkrnl_(dxgkrnl), sources_(sources), notification_(platform, dxgkrnl),
      timer_(platform, &AvVsyncTick, this), reporting_(false), running_(false), suspended_reporting_(false),
      reported_(0u), enables_(0u), window_start_(0u) {}

Status SyntheticVsync::initialize()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::initialize");
    return timer_.initialize();
}

Status SyntheticVsync::enable()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::enable");
    enables_++;
    reporting_ = true;
    /* The window opens at the first blank of a report, whichever enable
     * brought it. */
    if (reported_ == 0u) window_start_ = platform_.interrupt_time();
    if (running_) return Status::ok;
    const Status started = timer_.start(frame_period);
    running_ = started == Status::ok;
    return started;
}

/* The blank keeps its phase: only the reporting stops. */
void SyntheticVsync::disable()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::disable");
    reporting_ = false;
}

void SyntheticVsync::destroy()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::destroy");
    reporting_ = false;
    running_ = false;
    timer_.destroy();
}

void SyntheticVsync::suspend()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::suspend");
    suspended_reporting_ = reporting_;
    reporting_ = false;
    running_ = false;
    timer_.stop();
    /* Interrupt time counts the sleep, so a window that straddles the
     * transition measures the sleep and not the period the timer kept. */
    reported_ = 0u;
    enables_ = 0u;
}

Status SyntheticVsync::resume()
{
    platform_.require_irql_at_most(Irql::passive, "SyntheticVsync::resume");
    if (!suspended_reporting_) return Status::ok;
    suspended_reporting_ = false;
    return enable();
}

void SyntheticVsync::tick()
{
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::tick");
    if (!reporting_) return;
    if (notification_.report_vsync(target, sources_.scanout_address(target)) != Status::ok) {
        platform_.log("rvkgpu: a vsync was refused");
        return;
    }
    reported_++;
    if (reported_ == ticks_per_report) report_rate();
    dxgkrnl_.notify_dpc();
}

uint32_t SyntheticVsync::reported() const
{
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::reported");
    return reported_;
}

uint32_t SyntheticVsync::enabled_count() const
{
    platform_.require_irql_at_most(Irql::dispatch, "SyntheticVsync::enabled_count");
    return enables_;
}

void SyntheticVsync::report_rate()
{
    const uint64_t now = platform_.interrupt_time();
    char           bytes[96];
    TextBuffer     line(bytes, sizeof bytes);
    line.append("rvkgpu: vsync ");
    line.append_decimal(reported_);
    line.append(" blanks in ");
    line.append_decimal(now - window_start_);
    line.append(" ticks, enabled ");
    line.append_decimal(enables_);
    line.append(" times");
    platform_.log(line.text());
    window_start_ = now;
    reported_ = 0u;
    enables_ = 0u;
}

}  // namespace av
