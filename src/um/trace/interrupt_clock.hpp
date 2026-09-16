/* The interrupt time, in 100 ns, read from the page the kernel shares with
 * every process - the clock KeQueryInterruptTime reads, so a process's trace
 * table and the driver's are on one clock (docs/trace.md). No import:
 * QueryInterruptTime is newer than some guests' kernel32. Translation only. */
#ifndef AV_UM_INTERRUPT_CLOCK_HPP
#define AV_UM_INTERRUPT_CLOCK_HPP

#include <stdint.h>

namespace av {

class InterruptClock {
public:
    InterruptClock();

    uint64_t now() const;

private:
    /* KUSER_SHARED_DATA's InterruptTime, a KSYSTEM_TIME. */
    const uint32_t *time_;
};

}  // namespace av

#endif  // AV_UM_INTERRUPT_CLOCK_HPP
