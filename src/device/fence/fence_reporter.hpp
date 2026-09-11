/* Reports one context's completed batches to the guest: its fence page,
 * always written from the device's own counters, and one interrupt on the
 * context's line for each service that completed anything. Prepared on the
 * main loop at setup, before the ring is attached; used only by the worker
 * after that. */
#ifndef AV_FENCE_REPORTER_HPP
#define AV_FENCE_REPORTER_HPP

#include "core/support/status.hpp"
#include "device/fence/fence_window.hpp"

#include <stdint.h>

namespace av {

class FenceInterrupts;
class Host;
struct FenceReporterProbe;

class FenceReporter {
public:
    FenceReporter(Host &host, FenceInterrupts &interrupts);
    FenceReporter(const FenceReporter &) = delete;
    FenceReporter &operator=(const FenceReporter &) = delete;

    /* Main loop: writes the fresh page. invalid_parameter for a window that
     * cannot hold one or a line past the fence queues; invalid_state once
     * prepared. */
    [[nodiscard]] Status prepare(const FenceWindow &page, uint32_t line);
    /* Main loop: forgets a page whose ring was never attached. */
    void                 withdraw();
    /* Worker: the page reads `fence` completed, and progress moves on. */
    void                 complete(uint64_t fence);
    /* Worker: one interrupt when anything completed since the last. */
    void                 signal();

    uint64_t completed() const;
    uint64_t progress() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    FenceInterrupts &interrupts_;
    FenceWindow      page_;
    uint32_t         line_;
    uint64_t         completed_;
    uint64_t         progress_;
    bool             prepared_;
    bool             pending_;

    friend struct FenceReporterProbe;
};

}  // namespace av

#endif  // AV_FENCE_REPORTER_HPP
