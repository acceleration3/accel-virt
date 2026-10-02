/* What the MSVC ABI references that a C runtime would define, for binaries
 * that link none: _purecall, the target of a pure virtual call, which is in
 * every abstract vtable - and COM is nothing but abstract vtables - and
 * _fltused, which every object that touches floating point names and which
 * only says the floating-point support is linked. Every DLL takes this file
 * as it takes stack_probe.cpp; the programs take it from the avguest archive,
 * whose glob it is in. */
#include <windows.h>

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
