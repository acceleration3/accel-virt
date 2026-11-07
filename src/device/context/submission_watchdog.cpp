#include "device/context/submission_watchdog.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SubmissionWatchdog::SubmissionWatchdog(uint64_t limit_ns) : limit_ns_(limit_ns), since_ns_(0u), reached_(0u)
{
    AV_SCOPE("SubmissionWatchdog::SubmissionWatchdog");
}

void SubmissionWatchdog::waiting_at(uint64_t reached, uint64_t now)
{
    AV_SCOPE("SubmissionWatchdog::waiting_at");
    if (since_ns_ != 0u && reached == reached_) return;
    since_ns_ = now != 0u ? now : 1u;
    reached_ = reached;
}

void SubmissionWatchdog::nothing_outstanding()
{
    AV_SCOPE("SubmissionWatchdog::nothing_outstanding");
    since_ns_ = 0u;
}

bool SubmissionWatchdog::overran(uint64_t now) const
{
    AV_SCOPE("SubmissionWatchdog::overran");
    return limit_ns_ != 0u && since_ns_ != 0u && now > since_ns_ && now - since_ns_ >= limit_ns_;
}

uint64_t SubmissionWatchdog::limit() const
{
    AV_SCOPE("SubmissionWatchdog::limit");
    return limit_ns_;
}

}
