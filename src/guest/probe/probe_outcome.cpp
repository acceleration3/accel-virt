#include "guest/probe/probe_outcome.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/suite/scenario_steps.hpp"

namespace av {
ProbeOutcome::ProbeOutcome() : result_(VK_SUCCESS), missing_(false), matched_(0u), expected_(1u)
{
    AV_SCOPE("ProbeOutcome::ProbeOutcome");
}

bool ProbeOutcome::accept(VkResult result)
{
    AV_SCOPE("ProbeOutcome::accept");
    if (result == VK_SUCCESS) return true;
    if (result_ == VK_SUCCESS && !missing_) result_ = result;
    return false;
}

void ProbeOutcome::missing()
{
    AV_SCOPE("ProbeOutcome::missing");
    if (result_ == VK_SUCCESS) missing_ = true;
}

void ProbeOutcome::measure(uint32_t matched, uint32_t expected)
{
    AV_SCOPE("ProbeOutcome::measure");
    matched_ = matched;
    expected_ = expected;
}

void ProbeOutcome::report(ScenarioSteps &steps, const char *step) const
{
    AV_SCOPE("ProbeOutcome::report");
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
    AV_SCOPE("ProbeOutcome::invariants_hold");
    return !(missing_ && result_ != VK_SUCCESS) && matched_ <= expected_;
}

}
