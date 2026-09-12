/* How much the driver may say in one window. Refusals are logged at the point
 * of decision (constitution IX), and some of those points are the DPC at
 * DISPATCH, where a fault storm would otherwise turn one failure into an
 * unbounded stream of log lines. Lines are admitted up to a fixed count per
 * window; the rest are counted, so a later line can say how many were lost. */
#ifndef AV_LOG_BUDGET_HPP
#define AV_LOG_BUDGET_HPP

#include "core/platform/spin_lock.hpp"

#include <stdint.h>

namespace av {

class Platform;
struct LogBudgetProbe;

class LogBudget {
public:
    static constexpr uint32_t lines_per_window = 32u;
    /* One second, in the 100ns units Platform::interrupt_time counts. */
    static constexpr uint64_t window_ticks = 10000000u;

    explicit LogBudget(Platform &platform);
    LogBudget(const LogBudget &) = delete;
    LogBudget &operator=(const LogBudget &) = delete;

    /* Dispatch at most. Whether another line may be written now; a refusal is
     * counted, never logged. */
    [[nodiscard]] bool allows();
    uint64_t           dropped() const;

    bool invariants_hold() const;

private:
    Platform &platform_;
    SpinLock  lock_;
    uint64_t  window_start_;
    uint32_t  written_;
    uint64_t  dropped_;

    friend struct LogBudgetProbe;
};

}  // namespace av

#endif  // AV_LOG_BUDGET_HPP
