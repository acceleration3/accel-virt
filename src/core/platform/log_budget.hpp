#ifndef AV_LOG_BUDGET_HPP
#define AV_LOG_BUDGET_HPP

#include "core/platform/spin_lock.hpp"

#include <stdint.h>

namespace av {

class Platform;
struct LogBudgetProbe;

class LogBudget {
public:
    static constexpr uint32_t lines_per_window = 256u;
    static constexpr uint64_t window_ticks = 10000000u;

    explicit LogBudget(Platform &platform);
    LogBudget(const LogBudget &) = delete;
    LogBudget &operator=(const LogBudget &) = delete;

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

}

#endif
