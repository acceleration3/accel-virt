#ifndef AV_POLL_BUDGET_HPP
#define AV_POLL_BUDGET_HPP

#include <stdint.h>

namespace av {

class PollClock;

class PollBudget {
public:
    PollBudget(PollClock &clock, uint64_t spin_nanoseconds, uint64_t pause_nanoseconds,
               uint64_t deadline_nanoseconds);

    [[nodiscard]] bool next();
    bool               expired() const;

    bool invariants_hold() const;

private:
    PollClock &clock_;
    uint64_t   spin_nanoseconds_;
    uint64_t   pause_nanoseconds_;
    uint64_t   deadline_nanoseconds_;
    uint64_t   started_;
    bool       polled_;
    bool       expired_;

    friend struct PollBudgetProbe;
};

}

#endif
