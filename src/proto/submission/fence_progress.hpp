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

    [[nodiscard]] Status record_submission(uint64_t fence_id);
    void                 user_mode_submits();
    void                 driver_submits();
    [[nodiscard]] Status observe(const FencePage &page);

    uint64_t highest_submitted() const;
    uint64_t completed_fence() const;
    uint64_t progress() const;
    uint64_t raised() const;
    uint64_t beat() const;
    bool     faulted() const;

    bool invariants_hold() const;

private:
    uint64_t submitted_;
    bool     driver_submits_;
    uint64_t completed_;
    uint64_t progress_;
    uint64_t raised_;
    uint64_t beat_;
    bool     faulted_;

    friend struct FenceProgressProbe;
};

}

#endif
