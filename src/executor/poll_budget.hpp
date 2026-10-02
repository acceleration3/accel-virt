/* A wait on something the host's GPU or another thread finishes, polled: a
 * pause between polls, and a last poll after which the waiter gives up rather
 * than hang its caller - a hung GPU answers "still drawing" forever. */
#ifndef AV_POLL_BUDGET_HPP
#define AV_POLL_BUDGET_HPP

#include <stdint.h>

namespace av {

class PollBudget {
public:
    /* `most_polls` polls, `pause_nanoseconds` (under a second) apart. */
    PollBudget(uint32_t pause_nanoseconds, uint32_t most_polls);

    /* Whether another poll may be made, pausing first unless it is the
     * first; false once every poll has been. */
    [[nodiscard]] bool next();
    uint32_t           polls() const;

private:
    uint32_t pause_nanoseconds_;
    uint32_t most_polls_;
    uint32_t polls_;
};

}  // namespace av

#endif  // AV_POLL_BUDGET_HPP
