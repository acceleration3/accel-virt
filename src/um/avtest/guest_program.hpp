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
    enum class Run : uint8_t { suite, timeout, av, round_trip, host_loss };

    GuestProgram(uint32_t stress_seconds, Run run, ObjectStorage<ProcessTrace> &trace, ProcessTrace::Filter crashed);
    GuestProgram(const GuestProgram &) = delete;
    GuestProgram &operator=(const GuestProgram &) = delete;

    uint32_t run();

    void report(StateWriter &writer) override;

private:
    void write_report() const;

    GdiKmt      kmt_;
    ProcessHeap heap_;
    GuestReport report_;
    GuestSuite  suite_;
    Run         run_;
    ObjectStorage<ProcessTrace> &trace_;
    ProcessTrace::Filter         crashed_;
};

}

#endif
