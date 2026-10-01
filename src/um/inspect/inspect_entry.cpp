/* avinspect.exe's process entry (docs/trace.md), and what the MSVC ABI
 * references with no C runtime to supply it, as for avtest.exe. */
#include "um/inspect/inspect_session.hpp"

#include <windows.h>

extern "C" void avinspect_entry() { ExitProcess(av::InspectSession().run(GetCommandLineW())); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
