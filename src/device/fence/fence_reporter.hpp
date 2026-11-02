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

    [[nodiscard]] Status prepare(const FenceWindow &page, uint32_t line);
    void                 withdraw();
    void                 complete(uint64_t fence);
    void                 heartbeat();
    void                 raise();
    void                 fault(FaultCode code);
    void                 signal();
    void                 at_work(bool working);
    void                 beat_by(DevicePulse &pulse);
    bool                 working() const;
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
    bool             working_;
    bool             quiet_;

    friend struct FenceReporterProbe;
};

}

#endif
