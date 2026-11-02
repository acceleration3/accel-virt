#ifndef AV_UM_PROCESS_TRACE_HPP
#define AV_UM_PROCESS_TRACE_HPP

#include "core/support/status.hpp"
#include "core/trace/event_ring.hpp"
#include "core/trace/trace_table.hpp"
#include "um/common/diagnostics_directory.hpp"
#include "um/trace/event_file.hpp"
#include "um/trace/object_security.hpp"
#include "um/trace/state_service.hpp"
#include "um/trace/trace_section.hpp"
#include "um/trace/windows_crash.hpp"
#include "um/trace/windows_trace_host.hpp"

#include <stdint.h>

namespace av {

class StateReporter;

class ProcessTrace {
public:
    using Filter = long(__stdcall *)(void *pointers);

    ProcessTrace();
    ProcessTrace(const ProcessTrace &) = delete;
    ProcessTrace &operator=(const ProcessTrace &) = delete;

    [[nodiscard]] Status start(const char *module, StateReporter &reporter, Filter filter);
    long                 crashed(void *pointers);

private:
    void                 record_events(const char *module, uint32_t pid);

public:

private:
    WindowsTraceHost host_;
    ObjectSecurity   security_;
    TraceSection     section_;
    TraceTable       table_;
    DiagnosticsDirectory directory_;
    EventFile        events_file_;
    EventRing        events_;
    StateService     service_;
    WindowsCrash     crash_;
    const char      *module_;
    Filter           previous_;
};

}

#endif
