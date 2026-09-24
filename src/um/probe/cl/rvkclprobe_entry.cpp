/* rvkclprobe.exe's entry points: the process entry lld-link is told about, and
 * what the MSVC ABI references with no C runtime to define it, as for
 * rvkglprobe.exe (rvkglprobe_entry.cpp). The program takes no arguments: there
 * is one thing to prove and one way to prove it. */
#include "um/probe/cl/cl_probe_program.hpp"

#include <windows.h>

extern "C" void rvkclprobe_entry() { ExitProcess(av::ClProbeProgram().run()); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
