/* avclprobe.exe's process entry, which lld-link is told about. The program
 * takes no arguments: there is one thing to prove and one way to prove it. */
#include "um/probe/cl/cl_probe_program.hpp"

#include <windows.h>

extern "C" void avclprobe_entry() { ExitProcess(av::ClProbeProgram().run()); }
