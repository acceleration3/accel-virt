/* What the guest has submitted and seen completed on one context. The device
 * writes the fence page, so each snapshot is checked against this history:
 * nothing moves backwards, nothing unsubmitted completes, a fault stays. */
#ifndef AV_FENCE_PROGRESS_HPP
#define AV_FENCE_PROGRESS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class FencePage;
struct FenceProgressProbe;

class FenceProgress {
public:
    FenceProgress();

    /* Fence ids are strictly increasing from 1. */
    [[nodiscard]] Status record_submission(uint64_t fence_id);
    /* A doorbell was connected on this context (feature 049): submissions no
     * longer pass through the driver, so "nothing unsubmitted completes" is a
     * judgement it can no longer make - on Windows the last queued value is
     * dxgkrnl's, and the DDI never hands it to the KMD. What stays is that
     * nothing moves backwards and a fault stays. */
    void                 user_mode_submits();
    /* The doorbell is gone and submissions are the driver's again, from what
     * the device has already completed. */
    void                 driver_submits();
    /* invalid_state, keeping the last good page, for anything inconsistent. */
    [[nodiscard]] Status observe(const FencePage &page);

    uint64_t highest_submitted() const;
    uint64_t completed_fence() const;
    uint64_t progress() const;
    bool     faulted() const;

    bool invariants_hold() const;

private:
    uint64_t submitted_;
    bool     driver_submits_;
    uint64_t completed_;
    uint64_t progress_;
    bool     faulted_;

    friend struct FenceProgressProbe;
};

}  // namespace av

#endif  // AV_FENCE_PROGRESS_HPP
