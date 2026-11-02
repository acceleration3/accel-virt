#include "core/platform/log_budget.hpp"

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

LogBudget::LogBudget(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::log_budget),
      window_start_(platform.interrupt_time()), written_(0u), dropped_(0u)
      {
          AV_SCOPE("LogBudget::LogBudget");
      }

bool LogBudget::allows()
{
    AV_SCOPE("LogBudget::allows");
    platform_.require_irql_at_most(Irql::dispatch, "LogBudget::allows");
    SpinLockGuard guard(lock_);
    const uint64_t now = platform_.interrupt_time();
    if (now - window_start_ >= window_ticks) {
        window_start_ = now;
        written_ = 0u;
    }
    if (written_ >= lines_per_window) {
        dropped_++;
        return false;
    }
    written_++;
    return true;
}

uint64_t LogBudget::dropped() const
{
    AV_SCOPE("LogBudget::dropped");
    return dropped_;
}

bool LogBudget::invariants_hold() const
{
    AV_SCOPE("LogBudget::invariants_hold");
    return written_ <= lines_per_window;
}

}
