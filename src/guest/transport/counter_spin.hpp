/* Whether a wait that asks again at once, rather than sleeping, is still
 * within its budget: a fixed time on the guest's performance counter, counted
 * from the first time it is asked - most such waits end at their first look,
 * and a reading taken for nothing would be time lost to a caller timing its
 * own waits on the same counter. A counter that does not measure (frequency
 * 0) gives no budget at all, so nothing spins on it.
 *
 * Every guest spin is one: the transport's waits for a reply, for room in the
 * ring and the fence window, for the host to take the ring and for the GPU;
 * a poll's first millisecond (PollPause); a paging queue's patience. */
#ifndef AV_COUNTER_SPIN_HPP
#define AV_COUNTER_SPIN_HPP

#include <stdint.h>

namespace av {

class Kmt;

class CounterSpin {
public:
    static constexpr uint64_t microseconds_per_second = 1000000u;

    /* A budget of `microseconds` on `kmt`'s counter, not yet started. */
    CounterSpin(const Kmt &kmt, uint64_t microseconds);
    CounterSpin(const CounterSpin &) = delete;
    CounterSpin &operator=(const CounterSpin &) = delete;

    /* Whether the budget lasts now; the first ask starts it. */
    bool lasts();

    bool invariants_hold() const;

private:
    const Kmt &kmt_;
    uint64_t   microseconds_;
    /* The budget in counter ticks and where it started, once it has. */
    uint64_t   budget_;
    uint64_t   started_;
    bool       running_;
};

}  // namespace av

#endif  // AV_COUNTER_SPIN_HPP
