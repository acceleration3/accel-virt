/* What every graphics driver's root is built over, whichever API it serves:
 * the process heap, the driver's log file, D3DKMT through gdi32 and the
 * process's trace. Each driver library's Seams holds one first, so the root
 * and everything else it makes are built over it and gone before it; its
 * fallible half - the D3DKMT wait event every driver's waits need - is what
 * LazyDriver refuses a process for. Translation only. */
#ifndef AV_UM_DRIVER_SEAMS_HPP
#define AV_UM_DRIVER_SEAMS_HPP

#include "core/support/status.hpp"
#include "um/common/file_log.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/trace/process_trace.hpp"

namespace av {

class DriverSeams {
public:
    /* `stem` names the driver's log file (L"avvk") and outlives it. */
    explicit DriverSeams(const wchar_t *stem);
    DriverSeams(const DriverSeams &) = delete;
    DriverSeams &operator=(const DriverSeams &) = delete;

    [[nodiscard]] Status initialize();

    ProcessHeap  &heap();
    FileLog      &log();
    GdiKmt       &kmt();
    ProcessTrace &trace();

private:
    ProcessHeap  heap_;
    FileLog      log_;
    GdiKmt       kmt_;
    ProcessTrace trace_;
};

}  // namespace av

#endif  // AV_UM_DRIVER_SEAMS_HPP
