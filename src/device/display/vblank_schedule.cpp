#include "device/display/vblank_schedule.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/pages/vblank_record.hpp"

namespace av {

VblankSchedule::VblankSchedule()
    : microhz_(default_millihz * VblankRecord::microhz_per_millihz), epoch_(0u), epoch_sequence_(0u), sequence_(0u), last_(0u), running_(false)
{
    AV_SCOPE("VblankSchedule::VblankSchedule");
}

/* The period's whole nanoseconds, and its remainder spread over the blanks, so
 * that blank n lands where n exact periods put it. */
uint64_t VblankSchedule::due(uint64_t sequence) const
{
    AV_SCOPE("VblankSchedule::due");
    constexpr uint64_t whole = VblankRecord::nanoseconds_times_microhz;
    const uint64_t     count = sequence - epoch_sequence_;
    return epoch_ + count * (whole / microhz_) + count * (whole % microhz_) / microhz_;
}

void VblankSchedule::rebase(uint64_t sequence, uint64_t at)
{
    AV_SCOPE("VblankSchedule::rebase");
    epoch_sequence_ = sequence;
    epoch_ = at;
    last_ = at;
}

void VblankSchedule::start(uint64_t now)
{
    AV_SCOPE("VblankSchedule::start");
    rebase(sequence_, now);
    running_ = true;
}

bool VblankSchedule::running() const
{
    AV_SCOPE("VblankSchedule::running");
    return running_;
}

void VblankSchedule::retime(uint32_t millihz, uint64_t now)
{
    AV_SCOPE("VblankSchedule::retime");
    const uint64_t last_blank = last();
    microhz_ = millihz * VblankRecord::microhz_per_millihz;
    rebase(sequence_, last_blank);
    if (due(sequence_ + 1u) <= now) rebase(sequence_, now);
}

void VblankSchedule::align(uint32_t microhz, int64_t shift, uint64_t now)
{
    AV_SCOPE("VblankSchedule::align");
    const uint64_t last_blank = last();
    /* The consumer measured its shift against the grid it saw, the old one. */
    const uint64_t scheduled = due(sequence_ + 1u);
    microhz_ = microhz;
    if (!running_) return;
    const uint64_t period = VblankRecord::nanoseconds_times_microhz / microhz_;
    const uint64_t earlier = shift > 0 ? static_cast<uint64_t>(shift) : 0u;
    const uint64_t later = shift < 0 ? static_cast<uint64_t>(-(shift + 1)) + 1u : 0u;
    uint64_t       next = scheduled + later;
    /* A shift past the whole time since the epoch keeps the phase it asks for. */
    if (earlier > next) next += ((earlier - next) / period + 1u) * period;
    next -= earlier;
    const uint64_t earliest = last_blank + period / 2u > now ? last_blank + period / 2u : now + 1u;
    if (next < earliest) next += ((earliest - next + period - 1u) / period) * period;
    rebase(sequence_, next - period);
    last_ = last_blank;
}

uint32_t VblankSchedule::microhz() const
{
    AV_SCOPE("VblankSchedule::microhz");
    return microhz_;
}

uint32_t VblankSchedule::refresh_millihz() const
{
    AV_SCOPE("VblankSchedule::refresh_millihz");
    return static_cast<uint32_t>((static_cast<uint64_t>(microhz_) + VblankRecord::microhz_per_millihz / 2u) /
                                 VblankRecord::microhz_per_millihz);
}

uint64_t VblankSchedule::next() const
{
    AV_SCOPE("VblankSchedule::next");
    return due(sequence_ + 1u);
}

uint64_t VblankSchedule::advance(uint64_t now)
{
    AV_SCOPE("VblankSchedule::advance");
    if (!running_) return 0u;
    uint64_t passed = 0u;
    while (due(sequence_ + 1u) <= now) {
        if (passed == most_overdue) {
            rebase(sequence_, now);
            return passed;
        }
        sequence_++;
        passed++;
        last_ = due(sequence_);
    }
    return passed;
}

uint64_t VblankSchedule::sequence() const
{
    AV_SCOPE("VblankSchedule::sequence");
    return sequence_;
}

uint64_t VblankSchedule::last() const
{
    AV_SCOPE("VblankSchedule::last");
    return last_;
}

/* The epoch's place in the sequence is one the count has reached, and the
 * rate is one the protocol allows. */
bool VblankSchedule::invariants_hold() const
{
    AV_SCOPE("VblankSchedule::invariants_hold");
    return epoch_sequence_ <= sequence_ &&
           microhz_ >= VblankRecord::least_millihz * VblankRecord::microhz_per_millihz &&
           microhz_ <= VblankRecord::most_millihz * VblankRecord::microhz_per_millihz;
}

}  // namespace av
