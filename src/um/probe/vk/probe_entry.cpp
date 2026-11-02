#include "um/probe/vk/probe_program.hpp"

#include <windows.h>

extern "C" void avvkprobe_entry() { ExitProcess(av::ProbeProgram().run()); }
