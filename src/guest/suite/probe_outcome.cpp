#include "guest/suite/probe_outcome.hpp"

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {

/* Nothing measured is not a pass: one expected value was never read. */
ProbeOutcome::ProbeOutcome() : result_(VK_SUCCESS), missing_(false), matched_(0u), expected_(1u) {}

bool ProbeOutcome::accept(VkResult result)
{
    if (result == VK_SUCCESS) return true;
    if (result_ == VK_SUCCESS && !missing_) result_ = result;
    return false;
}

void ProbeOutcome::missing()
{
    if (result_ == VK_SUCCESS) missing_ = true;
}

void ProbeOutcome::measure(uint32_t matched, uint32_t expected)
{
    matched_ = matched;
    expected_ = expected;
}

void ProbeOutcome::report(ScenarioSteps &steps, const char *step) const
{
    if (result_ != VK_SUCCESS) {
        static_cast<void>(steps.check_result(step, result_));
        return;
    }
    if (missing_) {
        static_cast<void>(steps.check_measured(step, Status::not_found, 0u));
        return;
    }
    static_cast<void>(steps.check_measured(step, matched_ == expected_ ? Status::ok : Status::conflict, matched_));
}

bool ProbeOutcome::invariants_hold() const
{
    return !(missing_ && result_ != VK_SUCCESS) && matched_ <= expected_;
}

}  // namespace av
