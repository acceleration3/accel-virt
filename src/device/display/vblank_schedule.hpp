/* One head's vertical blanks: when each is due on the host's monotonic clock,
 * at the head's rate in millihertz, and how many have passed. The deadlines are
 * absolute - blank n is due n periods after the schedule's epoch, the period
 * kept to the fraction of a nanosecond the rate implies - so a clock thread
 * that wakes late still counts every blank that passed, and never drifts. */
#ifndef AV_VBLANK_SCHEDULE_HPP
#define AV_VBLANK_SCHEDULE_HPP

#include <stdint.h>

namespace av {


class VblankSchedule {
public:
    static constexpr uint32_t default_millihz = 60000u;
    /* More blanks overdue than this is a host that slept, not a thread that
     * woke late: the schedule starts again from now rather than counting a
     * suspend's worth of blanks one by one. */
    static constexpr uint64_t most_overdue = 1u << 20u;

    VblankSchedule();

    /* The first blank one period after `now`; the sequence carries on from
     * wherever it was. */
    void     start(uint64_t now);
    bool     running() const;
    /* From the next blank on, `millihz` apart: the blank after the last one
     * comes one new period after it, or one new period after `now` when that
     * is already past. The sequence carries on. */
    void     retime(uint32_t millihz, uint64_t now);
    /* From the next blank on, `millihz` apart and in phase with `blank`: each
     * a whole number of periods from it, on the host's monotonic clock. The
     * next blank is the first such one at least half a new period after the
     * last and after `now`, so the guest never sees two blanks closer than
     * that. Nothing but the rate is kept before the schedule starts, which
     * puts its first blank where start says. The sequence carries on. */
    void     align(uint32_t millihz, uint64_t blank, uint64_t now);
    uint32_t refresh_millihz() const;
    /* The deadline of the next blank. */
    uint64_t next() const;
    /* Counts every blank due at or before `now`; answers how many that was. */
    uint64_t advance(uint64_t now);
    /* Blanks counted since the schedule was made, the first 1. */
    uint64_t sequence() const;
    /* When the blank at sequence() was due. */
    uint64_t last() const;

    bool invariants_hold() const;

private:
    /* When blank `sequence` is due, from the epoch. */
    uint64_t due(uint64_t sequence) const;
    void     rebase(uint64_t sequence, uint64_t at);

    uint32_t millihz_;
    uint64_t epoch_;
    uint64_t epoch_sequence_;
    uint64_t sequence_;
    /* When blank sequence_ was due: the epoch's grid says so until align
     * moves the grid, and this goes on saying when it was. */
    uint64_t last_;
    bool     running_;
};

}  // namespace av

#endif  // AV_VBLANK_SCHEDULE_HPP
