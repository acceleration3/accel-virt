/* Reports one context's completions to the guest: its fence page, always
 * written from the device's own counters - progress for every command run and
 * batch completed, and a fault that, once written, is the page's last change -
 * and one interrupt on the context's line for each service that completed
 * anything or faulted. Prepared on the main loop at setup, before the ring is
 * attached; used only by the worker after that, but for the beat word, which
 * the device's pulse writes (DevicePulse) while the worker says it is at
 * work. */
#ifndef AV_FENCE_REPORTER_HPP
#define AV_FENCE_REPORTER_HPP

#include "core/support/status.hpp"
#include "device/fence/fence_window.hpp"
#include "proto/submission/fault_code.hpp"

#include <stdint.h>

namespace av {

class DevicePulse;
class FenceInterrupts;
class Host;
class WireRecorder;
struct FenceReporterProbe;

class FenceReporter {
public:
    FenceReporter(Host &host, FenceInterrupts &interrupts, WireRecorder &recorder, uint32_t context);
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
    /* Worker: progress moves on, with no interrupt. */
    void                 heartbeat();
    /* Worker: progress and raised move on and the guest is interrupted, with
     * nothing claimed completed - what a host event finishing on a thread the
     * device never gave the host reports (feature 052). raised counts these
     * alone, so the driver can tell one from a command's progress. */
    void                 raise();
    /* Worker: the page reads faulted with `code`; nothing is reported after. */
    void                 fault(FaultCode code);
    /* Worker: one interrupt when anything completed or faulted since the last. */
    void                 signal();
    /* Worker: whether it is at work for the guest now - inside a host call or
     * waiting on the host GPU - which is when the pulse beats; going to work
     * stirs the pulse that carries the page. */
    void                 at_work(bool working);
    /* The pulse, as it takes the page on: the one at_work stirs. It outlives
     * the reporter. */
    void                 beat_by(DevicePulse &pulse);
    /* Any thread. */
    bool                 working() const;
    /* The pulse, under its lock: the beat word one further. Nothing once
     * faulted, or before the page is prepared. */
    void                 beat();
    uint64_t             beats() const;
    bool                 faulted() const;

    uint64_t completed() const;
    uint64_t progress() const;
    uint64_t raised() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    FenceInterrupts &interrupts_;
    WireRecorder    &recorder_;
    uint32_t         context_;
    FenceWindow      page_;
    DevicePulse     *pulse_;
    uint32_t         line_;
    uint64_t         completed_;
    uint64_t         progress_;
    uint64_t         raised_;
    uint64_t         beats_;
    FaultCode        fault_;
    bool             prepared_;
    bool             faulted_;
    bool             pending_;
    /* Shared with the pulse's thread, so stored and read atomically. */
    bool             working_;
    bool             quiet_;

    friend struct FenceReporterProbe;
};

}  // namespace av

#endif  // AV_FENCE_REPORTER_HPP
