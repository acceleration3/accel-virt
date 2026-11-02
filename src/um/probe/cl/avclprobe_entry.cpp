#include "um/probe/cl/cl_probe_program.hpp"

#include <windows.h>

extern "C" void avclprobe_entry() { ExitProcess(av::ClProbeProgram().run()); }
