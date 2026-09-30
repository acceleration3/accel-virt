#include "core/display/vblank_phase.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VblankPhase::VblankPhase() : period_ns_(0u), anchor_(0), candidate_(0), seen_(0u), known_(false)
{
    AV_SCOPE("VblankPhase::VblankPhase");
}

/* The arithmetic is modular on purpose: the offsets are near each other, so
 * their signed differences order them wherever the counts started. */
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

bool VblankPhase::next(uint64_t now, uint64_t &sequence, uint64_t &at) const
{
    AV_SCOPE("VblankPhase::next");
    if (!known_) return false;
    const int64_t elapsed = static_cast<int64_t>(now * ns_per_tick) - anchor_;
    const int64_t period = static_cast<int64_t>(period_ns_);
    /* Rounded toward the blank after `now` whichever side of zero it is. */
    const int64_t count = elapsed >= 0 ? elapsed / period + 1 : -((-elapsed) / period);
    sequence = static_cast<uint64_t>(count);
    at = static_cast<uint64_t>(anchor_ + count * period) / ns_per_tick;
    return true;
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

/* A known phase has a period; an unknown one has nothing. */
bool VblankPhase::invariants_hold() const
{
    AV_SCOPE("VblankPhase::invariants_hold");
    return known_ ? period_ns_ != 0u && seen_ < window : period_ns_ == 0u && seen_ == 0u;
}

}  // namespace av
