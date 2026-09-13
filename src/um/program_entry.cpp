/* The two functions the Windows toolchain calls with no C runtime to supply
 * them: the process entry point lld-link is told about, and the target of a
 * pure virtual call, which the MSVC ABI puts in every abstract vtable.
 *
 * With no C runtime there is no argv: what rvktest takes - the seconds of fence
 * stress to run, 0 for none, and the word `tdr` for a run of the timeout
 * scenario alone - is read from the command line itself, which is the last run
 * of digits in it and whether that word is in it. */
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

/* Whether the command line asks for the timeout scenario instead of the suite
 * (feature 040, US3). */
static bool rvktest_timeout_only(const wchar_t *line)
{
    for (; *line != L'\0'; line++)
        if (line[0] == L' ' && line[1] == L't' && line[2] == L'd' && line[3] == L'r') return true;
    return false;
}

extern "C" void rvktest_entry()
{
    const wchar_t *const line = GetCommandLineW();
    ExitProcess(av::GuestProgram(rvktest_stress_seconds(line), rvktest_timeout_only(line)).run());
}

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }
