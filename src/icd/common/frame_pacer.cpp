#include "icd/common/frame_pacer.hpp"

#include "miniport/display/display_mode.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/support/log_sink.hpp"

namespace av::icd {

FramePacer::FramePacer(Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t device)
    : kmt_(kmt), log_(log), adapter_(adapter), device_(device), blanks_(0u), failures_(0u), due_(0u),
      read_at_(0u), read_position_(), blank_since_admitted_(true)
{
    AV_SCOPE("FramePacer::FramePacer");
}

void FramePacer::wait()
{
    AV_SCOPE("FramePacer::wait");
    if (adapter_ != 0u && kmt_.wait_for_vertical_blank(adapter_, device_, primary_source) == Status::ok) {
        blanks_++;
        return;
    }
    if (failures_++ == 0u) log_.write("avvk: FIFO present unpaced: the adapter's vertical blank wait failed");
}

/* A schedule of its own, a refresh apart from whenever the first frame came,
 * put Minecraft's shown frames 14 ms after the blank: a few milliseconds late
 * carried one past the next blank, and one refresh in nine showed no new frame
 * while the next showed two.
 *
 * A blank came between two presents when the scan-out is earlier in its
 * refresh at the second. A game presents far more often than the screen
 * refreshes, so that alone decides; presents all but a refresh apart may land
 * at the same place in theirs, and the device's refresh is not exactly the
 * nominal one, so those count as a blank apart. */
bool FramePacer::admits()
{
    AV_SCOPE("FramePacer::admits");
    uint64_t       frequency = 0u;
    const uint64_t now = kmt_.performance_counter(frequency);
    const uint64_t period = frequency / DisplayMode::refresh_hz;
    BeamPosition   position;
    if (adapter_ == 0u || kmt_.scan_line(adapter_, primary_source, position) != Status::ok)
        return admits_on_schedule(now, period);
    const uint64_t nearly_a_refresh = period - period / refresh_slack_parts;
    if (now - read_at_ >= nearly_a_refresh || position.earlier_than(read_position_)) blank_since_admitted_ = true;
    read_at_ = now;
    read_position_ = position;
    if (!blank_since_admitted_) return false;
    blank_since_admitted_ = false;
    return true;
}

bool FramePacer::admits_on_schedule(uint64_t now, uint64_t period)
{
    AV_SCOPE("FramePacer::admits_on_schedule");
    if (now < due_) return false;
    due_ = now - due_ < period ? due_ + period : now + period;
    return true;
}

uint64_t FramePacer::blanks() const
{
    AV_SCOPE("FramePacer::blanks");
    return blanks_;
}

uint64_t FramePacer::failures() const
{
    AV_SCOPE("FramePacer::failures");
    return failures_;
}

/* A blank is only ever counted on an adapter: with none, every wait fails. */
bool FramePacer::invariants_hold() const
{
    AV_SCOPE("FramePacer::invariants_hold");
    return adapter_ != 0u || blanks_ == 0u;
}

}  // namespace av::icd
