/* avoverlayprobe.exe's entry point: the one line the linker's /ENTRY names. No
 * C runtime, so nothing runs before or after it. */
#include "um/probe/overlay/overlay_probe_program.hpp"

#include <windows.h>

extern "C" void avoverlayprobe_entry();

extern "C" void avoverlayprobe_entry() { ExitProcess(av::OverlayProbeProgram().run()); }
