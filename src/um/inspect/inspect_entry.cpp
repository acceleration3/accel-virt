/* rvkinspect.exe's process entry (docs/trace.md), and what the MSVC ABI
 * references with no C runtime to supply it, as for rvktest.exe. */
#include "um/inspect/inspect_session.hpp"

#include <windows.h>

extern "C" void rvkinspect_entry() { ExitProcess(av::InspectSession().run(GetCommandLineW())); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" int _fltused = 0;
