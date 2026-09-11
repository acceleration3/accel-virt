/* The two functions the Windows toolchain calls with no C runtime to supply
 * them: the process entry point lld-link is told about, and the target of a
 * pure virtual call, which the MSVC ABI puts in every abstract vtable. */
#include "um/guest_program.hpp"

#include <windows.h>

extern "C" void rvktest_entry() { ExitProcess(av::GuestProgram().run()); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }
