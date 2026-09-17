/* rvkglprobe.exe's entry points: the process entry lld-link is told about,
 * and what the MSVC ABI references with no C runtime to define it, as for
 * rvktest.exe (program_entry.cpp). --discovery is the only run until T042. */
#include "um/probe/gl/gl_discovery_probe.hpp"

#include <windows.h>

extern "C" void rvkglprobe_entry() { ExitProcess(av::GlDiscoveryProbe().run()); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" int _fltused = 0;
