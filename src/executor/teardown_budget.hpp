#ifndef AV_TEARDOWN_BUDGET_HPP
#define AV_TEARDOWN_BUDGET_HPP

#include <stdint.h>

namespace av {

class PollClock;

class TeardownBudget {
public:
    TeardownBudget(const PollClock &clock, uint64_t nanoseconds);

    uint64_t remaining() const;
    bool     expired() const;

private:
    const PollClock &clock_;
    uint64_t         deadline_;
};

}

#endif
