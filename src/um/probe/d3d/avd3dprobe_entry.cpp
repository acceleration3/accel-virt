/* The three D3D probes' entry points (feature 047, T028). Each program is its
 * own executable, and each of these is the one line the linker's /ENTRY names:
 * no C runtime, so nothing runs before or after it.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * answer them: the first is in every abstract vtable - and COM is nothing but
 * abstract vtables - and the second is emitted wherever a float is used, which
 * a clear colour is. */
#include "um/probe/d3d/d3d_probe_program.hpp"

#include <windows.h>

extern "C" void avd3d9probe_entry();
extern "C" void avd3d11probe_entry();
extern "C" void avd3d12probe_entry();

extern "C" int  _purecall();
extern "C" int  _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }
extern "C" [[gnu::used]] int  _fltused = 0;

extern "C" void avd3d9probe_entry() { ExitProcess(av::D3d9ProbeProgram().run()); }
extern "C" void avd3d11probe_entry() { ExitProcess(av::D3d11ProbeProgram().run()); }
extern "C" void avd3d12probe_entry() { ExitProcess(av::D3d12ProbeProgram().run()); }
