#ifndef AV_GENEROUS_BUDGET_HPP
#define AV_GENEROUS_BUDGET_HPP

#include "executor/monotonic_clock.hpp"
#include "executor/teardown_budget.hpp"

namespace av {

class GenerousBudget {
public:
    GenerousBudget() : clock_(), budget_(clock_, 60000000000u) {}
    GenerousBudget(const GenerousBudget &) = delete;
    GenerousBudget &operator=(const GenerousBudget &) = delete;

    TeardownBudget &budget() { return budget_; }

private:
    MonotonicClock clock_;
    TeardownBudget budget_;
};

}

#endif
