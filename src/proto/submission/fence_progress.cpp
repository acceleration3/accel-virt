#include "proto/submission/fence_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

FenceProgress::FenceProgress()
    : submitted_(0u), driver_submits_(true), completed_(0u), progress_(0u), raised_(0u), beat_(0u), faulted_(false)
{
    AV_SCOPE("FenceProgress::FenceProgress");
}

void FenceProgress::user_mode_submits()
{
    AV_SCOPE("FenceProgress::user_mode_submits");
    driver_submits_ = false;
}

void FenceProgress::driver_submits()
{
    AV_SCOPE("FenceProgress::driver_submits");
    driver_submits_ = true;
    if (completed_ > submitted_) submitted_ = completed_;
}

Status FenceProgress::record_submission(uint64_t fence_id)
{
    AV_SCOPE("FenceProgress::record_submission");
    if (fence_id <= submitted_) return Status::invalid_state;
    submitted_ = fence_id;
    return Status::ok;
}

Status FenceProgress::observe(const FencePage &page)
{
    AV_SCOPE("FenceProgress::observe");
    const bool regressed =
        page.completed_fence() < completed_ || page.progress() < progress_ || page.raised() < raised_ ||
                           page.beat() < beat_;
    const bool unsubmitted = driver_submits_ && page.completed_fence() > submitted_;
    const bool unfaulted = faulted_ && !page.faulted();
    if (regressed || unsubmitted || unfaulted) return Status::invalid_state;
    completed_ = page.completed_fence();
    progress_ = page.progress();
    raised_ = page.raised();
    beat_ = page.beat();
    faulted_ = page.faulted();
    return Status::ok;
}

uint64_t FenceProgress::highest_submitted() const
{
    AV_SCOPE("FenceProgress::highest_submitted");
    return submitted_;
}
uint64_t FenceProgress::completed_fence() const
{
    AV_SCOPE("FenceProgress::completed_fence");
    return completed_;
}
uint64_t FenceProgress::progress() const
{
    AV_SCOPE("FenceProgress::progress");
    return progress_;
}
uint64_t FenceProgress::raised() const
{
    AV_SCOPE("FenceProgress::raised");
    return raised_;
}
uint64_t FenceProgress::beat() const
{
    AV_SCOPE("FenceProgress::beat");
    return beat_;
}
bool     FenceProgress::faulted() const
{
    AV_SCOPE("FenceProgress::faulted");
    return faulted_;
}

bool FenceProgress::invariants_hold() const
{
    AV_SCOPE("FenceProgress::invariants_hold");
    return !driver_submits_ || completed_ <= submitted_;
}

}
