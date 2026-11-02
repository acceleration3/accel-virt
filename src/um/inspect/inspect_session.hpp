#ifndef AV_UM_INSPECT_SESSION_HPP
#define AV_UM_INSPECT_SESSION_HPP

#include "guest/inspect/inspect_program.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/inspect/windows_inspect_host.hpp"
#include "um/common/process_heap.hpp"

#include <stdint.h>

namespace av {

class InspectSession {
public:
    InspectSession();
    InspectSession(const InspectSession &) = delete;
    InspectSession &operator=(const InspectSession &) = delete;

    uint32_t run(const wchar_t *line);

private:
    ProcessHeap        heap_;
    GdiKmt             kmt_;
    WindowsInspectHost host_;
    InspectProgram     program_;
};

}

#endif
