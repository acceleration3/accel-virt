#ifndef AV_UM_DRIVER_SEAMS_HPP
#define AV_UM_DRIVER_SEAMS_HPP

#include "core/log/keyed_log.hpp"
#include "core/support/status.hpp"
#include "um/common/file_log.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/trace/process_trace.hpp"

namespace av {

class DriverSeams {
public:
    DriverSeams(const wchar_t *stem, const char *prefix);
    DriverSeams(const DriverSeams &) = delete;
    DriverSeams &operator=(const DriverSeams &) = delete;

    [[nodiscard]] Status initialize();

    ProcessHeap  &heap();
    KeyedLog     &log();
    GdiKmt       &kmt();
    ProcessTrace &trace();

private:
    ProcessHeap  heap_;
    FileLog      file_;
    KeyedLog     log_;
    GdiKmt       kmt_;
    ProcessTrace trace_;
};

}

#endif
