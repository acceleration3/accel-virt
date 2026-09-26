/* Host-visible memory a process writes into: a CPU-visible allocation is
 * locked, a GuestPattern written through the pointer D3DKMTLock2 answers, read
 * back through that same pointer, and the allocation unlocked.
 *
 * The host reading the hostmem BAR back through QMP (feature 041, SC-003) can
 * tell what the guest wrote from what the region held before it. The bytes that
 * read back as written are the number on the report line, and a driver that
 * handed out a copy of the memory instead of the memory is a number below the
 * pattern's size. */
#ifndef AV_HOST_VISIBLE_SCENARIO_HPP
#define AV_HOST_VISIBLE_SCENARIO_HPP

#include "guest/suite/guest_pattern.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class HostVisibleScenario {
public:
    /* One page: the granularity VidMm maps a CPU-visible segment's views at. */
    static constexpr uint32_t pattern_bytes = 4096u;
    /* Enough of the view that the clock's own cost disappears in the rate. */
    static constexpr uint64_t measured_bytes = 16u * 1024u * 1024u;

    HostVisibleScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

    /* Bytes of the pattern that read back as they were written; zero until it
     * has held a view. */
    uint32_t matched_bytes() const;

private:
    /* MB/s writing and then reading the whole view: what the caching VidMm
     * chose for it costs a process. */
    void measure(volatile uint64_t *view);
    uint32_t megabytes_per_second(uint64_t ticks, uint64_t frequency) const;

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
    GuestPattern         pattern_;
    uint32_t             matched_;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_SCENARIO_HPP
