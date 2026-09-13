/* The two functions the Windows toolchain calls with no C runtime to supply
 * them: the process entry point lld-link is told about, and the target of a
 * pure virtual call, which the MSVC ABI puts in every abstract vtable.
 *
 * With no C runtime there is no argv: the one argument rvktest takes - the
 * seconds of fence stress to run, 0 for none - is read from the command line
 * itself, which is the last run of digits in it. */
#include "um/guest_program.hpp"

#include <windows.h>

static uint32_t rvktest_stress_seconds(const wchar_t *line)
{
    uint32_t seconds = 0u;
    bool         reading = false;

    for (; *line != L'\0'; line++) {
        if (*line >= L'0' && *line <= L'9') {
            seconds = (reading ? seconds : 0u) * 10u + static_cast<uint32_t>(*line - L'0');
            reading = true;
            continue;
        }
        if (*line == L' ') reading = false;
    }
    return seconds;
}

extern "C" void rvktest_entry()
{
    ExitProcess(av::GuestProgram(rvktest_stress_seconds(GetCommandLineW())).run());
}

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }
