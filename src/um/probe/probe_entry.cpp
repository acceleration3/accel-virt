/* rvkvkprobe.exe's entry points: the process entry lld-link is told about,
 * and what the MSVC ABI references with no C runtime to define it, as for
 * rvktest.exe (program_entry.cpp). */
#include "um/probe/probe_program.hpp"

#include <windows.h>

extern "C" void rvkvkprobe_entry() { ExitProcess(av::ProbeProgram().run()); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
