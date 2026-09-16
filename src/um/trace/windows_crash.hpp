/* What a process's unhandled-exception filter does (docs/trace.md): the crash
 * goes into the trace table, the faulting thread's stack is unwound from the
 * exception's context, and CrashReport's lines are written to
 * `C:\ProgramData\rvk\crash-<module>-<pid>.txt`. Nothing is allocated; the
 * process is about to die. Translation only. */
#ifndef AV_UM_WINDOWS_CRASH_HPP
#define AV_UM_WINDOWS_CRASH_HPP

#include "guest/trace/crash_report.hpp"
#include "um/trace/windows_module_resolver.hpp"

#include <stdint.h>

namespace av {

class TraceHost;
class TraceTable;

class WindowsCrash {
public:
    WindowsCrash();
    WindowsCrash(const WindowsCrash &) = delete;
    WindowsCrash &operator=(const WindowsCrash &) = delete;

    /* `pointers` is the filter's EXCEPTION_POINTERS; `anchor` any address in
     * the binary whose names the table holds. */
    void write(const char *module, TraceTable &table, TraceHost &host, void *pointers, uint64_t anchor);

private:
    /* The faulting thread's return addresses, from a copy of its context. */
    uint32_t unwind(void *pointers);

    WindowsModuleResolver modules_;
    uint64_t              returns_[CrashReport::max_frames];
};

}  // namespace av

#endif  // AV_UM_WINDOWS_CRASH_HPP
