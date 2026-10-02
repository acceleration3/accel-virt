/* avinspect.exe's process entry (docs/trace.md). */
#include "um/inspect/inspect_session.hpp"

#include <windows.h>

extern "C" void avinspect_entry() { ExitProcess(av::InspectSession().run(GetCommandLineW())); }
