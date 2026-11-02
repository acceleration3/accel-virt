#include "device/feasibility/probe_result.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

ProbeResult::ProbeResult() : outcome_(Outcome::not_run), reason_(Status::ok), errno_(0), measured_(0u)
{
    AV_SCOPE("ProbeResult::ProbeResult");
}

Status ProbeResult::record_pass(uint64_t measured)
{
    AV_SCOPE("ProbeResult::record_pass");
    if (outcome_ != Outcome::not_run) return Status::invalid_state;
    outcome_ = Outcome::passed;
    measured_ = measured;
    return Status::ok;
}

Status ProbeResult::record_failure(Status reason, int32_t host_error)
{
    AV_SCOPE("ProbeResult::record_failure");
    if (outcome_ != Outcome::not_run) return Status::invalid_state;
    outcome_ = Outcome::failed;
    reason_ = reason;
    errno_ = host_error;
    return Status::ok;
}

void ProbeResult::measure(uint64_t measured)
{
    AV_SCOPE("ProbeResult::measure");
    if (outcome_ != Outcome::failed) return;
    measured_ = measured;
}

bool ProbeResult::ran() const
{
    AV_SCOPE("ProbeResult::ran");
    return outcome_ != Outcome::not_run;
}

bool ProbeResult::passed() const
{
    AV_SCOPE("ProbeResult::passed");
    return outcome_ == Outcome::passed;
}

Status ProbeResult::reason() const
{
    AV_SCOPE("ProbeResult::reason");
    return reason_;
}

int32_t ProbeResult::errno_value() const
{
    AV_SCOPE("ProbeResult::errno_value");
    return errno_;
}

uint64_t ProbeResult::measured() const
{
    AV_SCOPE("ProbeResult::measured");
    return measured_;
}

void ProbeResult::describe(TextBuffer &line) const
{
    AV_SCOPE("ProbeResult::describe");
    if (outcome_ == Outcome::not_run) {
        line.append("not run");
        return;
    }
    if (outcome_ == Outcome::passed) {
        line.append("passed, ");
        line.append_decimal(measured_);
        return;
    }
    line.append("failed, status ");
    line.append_decimal(static_cast<uint64_t>(reason_));
    if (errno_ == 0) return;
    line.append(", host ");
    if (errno_ < 0) {
        line.append("-");
        line.append_decimal(static_cast<uint64_t>(-static_cast<int64_t>(errno_)));
        return;
    }
    line.append_decimal(static_cast<uint64_t>(errno_));
}

}
