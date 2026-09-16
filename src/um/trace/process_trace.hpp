/* A Windows binary's tracing (docs/trace.md): its trace table in a named
 * section, bound for every AV_SCOPE in the binary, its state service and its
 * crash file. Lives in the binary's process-lifetime storage beside its root,
 * and is never torn down: a thread may be inside a scope until the process
 * ends, so the module is pinned against a loader's FreeLibrary. Translation
 * only. */
#ifndef AV_UM_PROCESS_TRACE_HPP
#define AV_UM_PROCESS_TRACE_HPP

#include "core/support/status.hpp"
#include "core/trace/trace_table.hpp"
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
    /* The unhandled-exception filter the binary's entry translation unit
     * defines; it forwards to crashed(). */
    using Filter = long(__stdcall *)(void *pointers);

    ProcessTrace();
    ProcessTrace(const ProcessTrace &) = delete;
    ProcessTrace &operator=(const ProcessTrace &) = delete;

    /* Lays the table out, binds it, pins this module, installs `filter` and
     * starts the state service; the first failure, with tracing left off if
     * the table could not be made. `module` is a literal that outlives the
     * process. */
    [[nodiscard]] Status start(const char *module, StateReporter &reporter, Filter filter);
    /* From the filter: the crash file, then whatever the filter replaced says. */
    long                 crashed(void *pointers);

private:
    WindowsTraceHost host_;
    ObjectSecurity   security_;
    TraceSection     section_;
    TraceTable       table_;
    StateService     service_;
    WindowsCrash     crash_;
    const char      *module_;
    Filter           previous_;
};

}  // namespace av

#endif  // AV_UM_PROCESS_TRACE_HPP
