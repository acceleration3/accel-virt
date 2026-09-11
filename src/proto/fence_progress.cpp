#include "fence_progress.hpp"

#include "fence_page.hpp"

namespace av {

FenceProgress::FenceProgress() : submitted_(0u), completed_(0u), progress_(0u), faulted_(false) {}

Status FenceProgress::record_submission(uint64_t fence_id)
{
    if (fence_id <= submitted_) return Status::invalid_state;
    submitted_ = fence_id;
    return Status::ok;
}

Status FenceProgress::observe(const FencePage &page)
{
    const bool regressed = page.completed_fence() < completed_ || page.progress() < progress_;
    const bool unsubmitted = page.completed_fence() > submitted_;
    const bool unfaulted = faulted_ && !page.faulted();
    if (regressed || unsubmitted || unfaulted) return Status::invalid_state;
    completed_ = page.completed_fence();
    progress_ = page.progress();
    faulted_ = page.faulted();
    return Status::ok;
}

uint64_t FenceProgress::highest_submitted() const { return submitted_; }
uint64_t FenceProgress::completed_fence() const   { return completed_; }
uint64_t FenceProgress::progress() const          { return progress_; }
bool     FenceProgress::faulted() const           { return faulted_; }

bool FenceProgress::invariants_hold() const { return completed_ <= submitted_; }

}  // namespace av
