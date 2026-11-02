#include "miniport/display/vblank_phase.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VblankPhase::VblankPhase() : period_ns_(0u), anchor_(0), candidate_(0), seen_(0u), known_(false)
{
    AV_SCOPE("VblankPhase::VblankPhase");
}

void VblankPhase::observe(uint64_t sequence, uint64_t now, uint32_t period_ns)
{
    AV_SCOPE("VblankPhase::observe");
    if (period_ns == 0u) return;
    const int64_t offset = static_cast<int64_t>(now * ns_per_tick - sequence * period_ns);
    if (!known_ || period_ns != period_ns_) {
        period_ns_ = period_ns;
        anchor_ = offset;
        candidate_ = offset;
        seen_ = 1u;
        known_ = true;
        return;
    }
    if (offset - anchor_ < 0) anchor_ = offset;
    candidate_ = seen_ == 0u || offset - candidate_ < 0 ? offset : candidate_;
    seen_++;
    if (seen_ < window) return;
    anchor_ = candidate_;
    seen_ = 0u;
}

bool VblankPhase::known() const
{
    AV_SCOPE("VblankPhase::known");
    return known_;
}

uint64_t VblankPhase::since_blank(uint64_t now) const
{
    AV_SCOPE("VblankPhase::since_blank");
    if (!known_) return 0u;
    const int64_t  elapsed = static_cast<int64_t>(now * ns_per_tick) - anchor_;
    const int64_t  period = static_cast<int64_t>(period_ns_);
    const int64_t  into = ((elapsed % period) + period) % period;
    return static_cast<uint64_t>(into) / ns_per_tick;
}

uint64_t VblankPhase::period() const
{
    AV_SCOPE("VblankPhase::period");
    return known_ ? period_ns_ / ns_per_tick : 0u;
}

void VblankPhase::forget()
{
    AV_SCOPE("VblankPhase::forget");
    period_ns_ = 0u;
    anchor_ = 0;
    candidate_ = 0;
    seen_ = 0u;
    known_ = false;
}

bool VblankPhase::invariants_hold() const
{
    AV_SCOPE("VblankPhase::invariants_hold");
    return known_ ? period_ns_ != 0u && seen_ < window : period_ns_ == 0u && seen_ == 0u;
}

}
