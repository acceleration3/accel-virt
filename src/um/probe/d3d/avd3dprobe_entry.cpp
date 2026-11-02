#include "um/probe/d3d/d3d_probe_program.hpp"

#include <windows.h>

extern "C" void avd3d9probe_entry();
extern "C" void avd3d11probe_entry();
extern "C" void avd3d12probe_entry();

extern "C" void avd3d9probe_entry() { ExitProcess(av::D3d9ProbeProgram().run()); }
extern "C" void avd3d11probe_entry() { ExitProcess(av::D3d11ProbeProgram().run()); }
extern "C" void avd3d12probe_entry() { ExitProcess(av::D3d12ProbeProgram().run()); }
