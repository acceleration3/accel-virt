/* US3 of feature 039: do fence interrupts survive the guest masking and
 * unmasking its own interrupt, at the rate this host sustains? Unlike the
 * memory probes this one does not run at realize - it watches a run. The device
 * tells it what it raised, the guest's report tells it what was seen, and it
 * judges the pair when the run ends.
 *
 * The two failures worth a long run are the ones a short one hides: an
 * interrupt injected while the guest had its interrupt disabled, and a final
 * fence the guest never saw - the wait that never wakes. */
#ifndef AV_INTERRUPT_STRESS_PROBE_HPP
#define AV_INTERRUPT_STRESS_PROBE_HPP

#include "device/feasibility/probe_result.hpp"

#include <stdint.h>

namespace av {

class Host;

class InterruptStressProbe {
public:
    explicit InterruptStressProbe(Host &host);
    ~InterruptStressProbe();
    InterruptStressProbe(const InterruptStressProbe &) = delete;
    InterruptStressProbe &operator=(const InterruptStressProbe &) = delete;

    /* Main loop. no_memory when the host has no mutex to hold the workers'
     * counts apart from the guest's reports. */
    [[nodiscard]] Status initialize();

    /* Any thread: one fence interrupt the device injected on `line`. */
    void raised(uint32_t line);
    /* Main loop: the guest disabled its interrupt, and enabled it again. */
    void disabled();
    void enabled();
    /* Main loop: what the guest says it has seen so far, and how long the
     * slowest interrupt of the run took to reach it. */
    void acknowledged(uint64_t seen, uint64_t nanoseconds);
    /* Main loop: the run is over; judge it, once. */
    void finish();

    const ProbeResult &verdict() const;

private:
    Host       &host_;
    void       *lock_;
    uint64_t    raised_;
    uint64_t    seen_;
    uint64_t    worst_;
    uint64_t    injected_while_disabled_;
    bool        listening_;
    ProbeResult result_;
};

}  // namespace av

#endif  // AV_INTERRUPT_STRESS_PROBE_HPP
