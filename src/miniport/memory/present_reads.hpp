/* The reads open on one allocation's presented pages, which the presents into
 * them wait for (docs/stale-blt-frames.md): a reader's begin opens one, its
 * end closes it, and a present is admitted to write only while none is open,
 * so a read never sees a present begin and its band is never half a frame.
 *
 * A reader that dies between its begin and its end never closes its read, and
 * nothing tells the driver it died, so a read is open for `expiry_ticks` and
 * no longer: a present then writes as if it were closed, and the next begin
 * forgets it. A read that outlives its expiry is the reader's to notice - the
 * write sequence moved under it. */
#ifndef AV_PRESENT_READS_HPP
#define AV_PRESENT_READS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"

#include <stdint.h>

namespace av {

class PresentWrites;
struct PresentReadsProbe;

class PresentReads {
public:
    /* A read is a memcmp and a copy of the rows that changed, about a
     * millisecond; a hundred is a reader that is gone. */
    static constexpr uint64_t expiry_ticks = 1000000u;   /* 100 ms, in 100 ns */

    explicit PresentReads(Platform &platform);
    PresentReads(const PresentReads &) = delete;
    PresentReads &operator=(const PresentReads &) = delete;

    /* DISPATCH at most. A read begins: presents are held from now until its
     * end or its expiry. Reads that had expired are forgotten first; says
     * whether there were any. */
    [[nodiscard]] bool open();
    /* DISPATCH at most. A read ends; one that was forgotten, or never
     * opened, is no error. */
    void               close();
    /* DISPATCH at most. Opens `writes` for a present and says so when no read
     * is open; says no and touches nothing when one is. The look and the
     * opening are one step, so a read that opens after it finds the present
     * open and waits for it. */
    [[nodiscard]] bool admit(PresentWrites &writes);

    bool invariants_hold();

private:
    /* Whether a read is open and not yet expired, at `now`. Under the lock. */
    bool holding(uint64_t now) const;

    Platform &platform_;
    SpinLock  lock_;
    uint64_t  open_;
    uint64_t  expires_;

    friend struct PresentReadsProbe;
};

}  // namespace av

#endif  // AV_PRESENT_READS_HPP
