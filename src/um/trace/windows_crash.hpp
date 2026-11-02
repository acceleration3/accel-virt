#ifndef AV_UM_WINDOWS_CRASH_HPP
#define AV_UM_WINDOWS_CRASH_HPP

#include "core/trace/crash_report.hpp"
#include "um/common/diagnostics_directory.hpp"
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

    void write(const char *module, TraceTable &table, TraceHost &host, void *pointers, uint64_t anchor);

private:
    uint32_t unwind(void *pointers);

    DiagnosticsDirectory  directory_;
    WindowsModuleResolver modules_;
    uint64_t              returns_[CrashReport::max_frames];
};

}

#endif
