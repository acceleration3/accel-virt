/* avtest.exe: the in-guest suite over gdi32, its report written to standard
 * output for the guest agent to carry back, its failures the exit code. */
#ifndef AV_GUEST_PROGRAM_HPP
#define AV_GUEST_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "core/support/object_storage.hpp"
#include "core/trace/state_reporter.hpp"
#include "guest/suite/guest_suite.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

namespace av {

class GuestProgram final : public StateReporter {
public:
    /* What the command line asked the program to run. */
    enum class Run : uint8_t { suite, timeout, av, round_trip, host_loss };

    /* The whole suite, with the long fence run only when the command line asks
     * for it: `avtest.exe <seconds>` (feature 039, US3). With `timeout_only`
     * the command line asked for `tdr` instead, and only the timeout scenario
     * runs: the host has been told to hold a context, and every other scenario
     * would be measuring that (feature 040, US3). With `av` only the scenarios
     * that send work to the host's executor library run (feature 044). */
    GuestProgram(uint32_t stress_seconds, Run run, ObjectStorage<ProcessTrace> &trace, ProcessTrace::Filter crashed);
    GuestProgram(const GuestProgram &) = delete;
    GuestProgram &operator=(const GuestProgram &) = delete;

    /* The process exit code: how many steps failed. */
    uint32_t run();

    /* `program/0 run=<suite|timeout|av>`, then the report's progress. */
    void report(StateWriter &writer) override;

private:
    void write_report() const;

    GdiKmt      kmt_;
    ProcessHeap heap_;
    GuestReport report_;
    GuestSuite  suite_;
    Run         run_;
    /* avtest's tracing, in the entry's storage (constitution III). */
    ObjectStorage<ProcessTrace> &trace_;
    ProcessTrace::Filter         crashed_;
};

}  // namespace av

#endif  // AV_GUEST_PROGRAM_HPP
