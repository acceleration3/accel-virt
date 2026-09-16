/* rvkinspect.exe as one object on the entry's stack: the process heap, D3DKMT
 * and the Windows host under InspectProgram, and the usage text written to
 * standard output when the command was not understood. Translation only. */
#ifndef AV_UM_INSPECT_SESSION_HPP
#define AV_UM_INSPECT_SESSION_HPP

#include "guest/inspect/inspect_program.hpp"
#include "um/gdi_kmt.hpp"
#include "um/inspect/windows_inspect_host.hpp"
#include "um/icd/process_heap.hpp"

#include <stdint.h>

namespace av {

class InspectSession {
public:
    InspectSession();
    InspectSession(const InspectSession &) = delete;
    InspectSession &operator=(const InspectSession &) = delete;

    /* The process exit code. */
    uint32_t run(const wchar_t *line);

private:
    ProcessHeap        heap_;
    GdiKmt             kmt_;
    WindowsInspectHost host_;
    InspectProgram     program_;
};

}  // namespace av

#endif  // AV_UM_INSPECT_SESSION_HPP
