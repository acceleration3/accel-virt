/* When the device's vertical blanks fall on the guest's clock. The device
 * counts blanks on the host's clock; the guest only sees each one when its
 * interrupt is handled, which is never early and sometimes late. So the phase
 * is taken from the earliest arrivals: each observation says blank n happened
 * at or before the time it was seen, and the smallest (time - n periods) in a
 * window of them is the phase - taken afresh each window, so a guest clock
 * drifting against the host's is followed rather than held. */
#ifndef AV_VBLANK_PHASE_HPP
#define AV_VBLANK_PHASE_HPP

#include <stdint.h>

namespace av {

class VblankPhase {
public:
    /* Observations a phase is taken over. */
    static constexpr uint32_t window = 64u;

    VblankPhase();

    /* Blank `sequence`, of a head whose period is `period_ns`, had happened by
     * `now` (100 ns units, the interrupt time). A new period starts over. */
    void     observe(uint64_t sequence, uint64_t now, uint32_t period_ns);
    bool     known() const;
    /* 100 ns units since the latest blank at `now`, below period(); 0 while
     * no phase is known. */
    uint64_t since_blank(uint64_t now) const;
    /* The period in 100 ns units, rounded down; 0 while no phase is known. */
    uint64_t period() const;
    /* The blank after the one at `now`: its sequence and its time, 100 ns
     * units. False while no phase is known. */
    bool     next(uint64_t now, uint64_t &sequence, uint64_t &at) const;
    void     forget();

    bool invariants_hold() const;

private:
    static constexpr uint64_t ns_per_tick = 100u;

    uint32_t period_ns_;
    /* Nanoseconds on the guest's clock at which blank 0 would have come, as
     * the phase stands and as the current window's best has it. */
    int64_t  anchor_;
    int64_t  candidate_;
    uint32_t seen_;
    bool     known_;
};

}  // namespace av

#endif  // AV_VBLANK_PHASE_HPP
