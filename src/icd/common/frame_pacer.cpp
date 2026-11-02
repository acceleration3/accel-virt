#include "icd/common/frame_pacer.hpp"

#include "miniport/display/display_mode.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/log/driver_log.hpp"

namespace av::icd {

FramePacer::FramePacer(Kmt &kmt, DriverLog &log, uint32_t adapter, uint32_t device)
    : kmt_(kmt), log_(log), adapter_(adapter), device_(device), blanks_(0u), failures_(0u), refresh_millihz_(0u), refresh_read_ticks_(0u), due_(0u),
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
    if (failures_++ == 0u) log_.write("presents unpaced: the adapter's vertical blank wait failed");
}

bool FramePacer::admits()
{
    AV_SCOPE("FramePacer::admits");
    uint64_t       frequency = 0u;
    const uint64_t now = kmt_.performance_counter(frequency);
    const uint64_t period = refresh_period(now, frequency);
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

uint64_t FramePacer::refresh_period(uint64_t now, uint64_t frequency)
{
    AV_SCOPE("FramePacer::refresh_period");
    if (refresh_millihz_ == 0u || now - refresh_read_ticks_ >= frequency) {
        uint32_t millihz = 0u;
        if (adapter_ != 0u && kmt_.refresh_millihz(adapter_, primary_source, millihz) == Status::ok && millihz != 0u)
            refresh_millihz_ = millihz;
        else if (refresh_millihz_ == 0u)
            refresh_millihz_ = DisplayMode::default_refresh_millihz;
        refresh_read_ticks_ = now;
    }
    return frequency * 1000u / refresh_millihz_;
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

bool FramePacer::invariants_hold() const
{
    AV_SCOPE("FramePacer::invariants_hold");
    return adapter_ != 0u || blanks_ == 0u;
}

}
