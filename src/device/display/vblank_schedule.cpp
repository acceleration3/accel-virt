#include "device/display/vblank_schedule.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/display/vblank_record.hpp"

namespace av {

VblankSchedule::VblankSchedule()
    : millihz_(default_millihz), epoch_(0u), epoch_sequence_(0u), sequence_(0u), running_(false)
{
    AV_SCOPE("VblankSchedule::VblankSchedule");
}

/* The period's whole nanoseconds, and its remainder spread over the blanks, so
 * that blank n lands where n exact periods put it. */
uint64_t VblankSchedule::due(uint64_t sequence) const
{
    AV_SCOPE("VblankSchedule::due");
    constexpr uint64_t whole = VblankRecord::nanoseconds_times_millihz;
    const uint64_t     count = sequence - epoch_sequence_;
    return epoch_ + count * (whole / millihz_) + count * (whole % millihz_) / millihz_;
}

void VblankSchedule::rebase(uint64_t sequence, uint64_t at)
{
    AV_SCOPE("VblankSchedule::rebase");
    epoch_sequence_ = sequence;
    epoch_ = at;
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
    millihz_ = millihz;
    rebase(sequence_, last_blank);
    if (due(sequence_ + 1u) <= now) rebase(sequence_, now);
}

uint32_t VblankSchedule::refresh_millihz() const
{
    AV_SCOPE("VblankSchedule::refresh_millihz");
    return millihz_;
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
    return due(sequence_);
}

/* The epoch is a blank the sequence has reached, and the rate is one the
 * protocol allows. */
bool VblankSchedule::invariants_hold() const
{
    AV_SCOPE("VblankSchedule::invariants_hold");
    return epoch_sequence_ <= sequence_ && millihz_ >= VblankRecord::least_millihz &&
           millihz_ <= VblankRecord::most_millihz;
}

}  // namespace av
