/* Whether the host is still at work on a context its process is waiting on,
 * judged only from what the device publishes (docs/protocol.md, "Fence
 * page"): the ring's head, which moves as each batch ends; the page's
 * completed fence and progress, which move as the device finishes batches
 * and runs commands; and its beat, which the device's pulse moves while
 * anything on the device is at work, a host call that blocks for seconds
 * included.
 *
 * How long the host takes is no evidence of anything: a host driver may
 * hold one call until the GPU drains a queue of seconds, and the host kernel
 * resets a GPU that is truly hung, which reaches the guest as a fault. So a
 * wait gives up only on a fault, or on silence - nothing published moving at
 * all for longer than any device that is alive stays quiet. */
#ifndef AV_HOST_WATCH_HPP
#define AV_HOST_WATCH_HPP

#include <stdint.h>

namespace av {

class Kmt;
class TransportRing;
struct HostWatchProbe;

class HostWatch {
public:
    /* Many of the pulse's periods (DevicePulse::period_ns): a device this
     * quiet is beating nothing, so it has no work at all, and what is waited
     * for will not come. */
    static constexpr uint64_t silence_seconds = 30u;

    HostWatch(Kmt &kmt, const TransportRing &ring);
    HostWatch(const HostWatch &) = delete;
    HostWatch &operator=(const HostWatch &) = delete;

    /* The context's fence page, read-only, as the driver mapped it; null from
     * a driver that maps none, which leaves only the ring's head to watch. */
    void     read_page(const uint8_t *page);
    bool     reads_page() const;
    /* The page's completed fence: 0 without a page. */
    uint64_t completed() const;
    /* Whether the page says the device faulted the context. */
    bool     faulted() const;

    /* A wait begins: what is published now is the baseline, and the silence
     * is counted from the first look. */
    void     start();
    /* Whether the wait goes on: no once the device faulted the context, or
     * once nothing it publishes has moved for silence_seconds. */
    bool     keeps_waiting();
    /* Performance-counter ticks from the first look to the last, and how many
     * make a second. */
    uint64_t elapsed() const;
    uint64_t frequency() const;

    bool invariants_hold() const;

private:
    /* Whether anything published moved since it was last seen, which it now
     * has been. */
    bool     moved();
    uint64_t word(uint32_t offset) const;

    Kmt                 &kmt_;
    const TransportRing &ring_;
    const uint8_t       *page_;
    uint64_t             head_;
    uint64_t             completed_;
    uint64_t             progress_;
    uint64_t             beat_;
    uint64_t             started_;
    uint64_t             last_moved_;
    uint64_t             now_;
    uint64_t             frequency_;
    /* Whether this wait has read the clock yet. */
    bool                 timing_;

    friend struct HostWatchProbe;
};

}  // namespace av

#endif  // AV_HOST_WATCH_HPP
