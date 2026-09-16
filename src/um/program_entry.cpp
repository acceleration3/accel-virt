/* What the Windows toolchain references with no C runtime to supply it: the
 * process entry point lld-link is told about, the target of a pure virtual
 * call, which the MSVC ABI puts in every abstract vtable, and _fltused, which
 * every object that touches floating point names (the OpenCL client's
 * cl_float arguments) and which only says the floating-point support is linked.
 *
 * With no C runtime there is no argv: what rvktest takes - the seconds of fence
 * stress to run, 0 for none, the word `tdr` for a run of the timeout scenario
 * alone and the word `rvk` for the rvk scenarios alone - is read from the
 * command line itself, which is the last run of digits in it and whether one of
 * those words is in it. */
#include "core/trace/function_scope.hpp"
#include "um/guest_program.hpp"

#include <windows.h>

static uint32_t rvktest_stress_seconds(const wchar_t *line)
{
    AV_SCOPE("rvktest_stress_seconds");
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

/* Whether the command line asks for the timeout scenario (feature 040, US3) or
 * the rvk scenarios (feature 044) instead of the suite. */
static av::GuestProgram::Run rvktest_run(const wchar_t *line)
{
    AV_SCOPE("rvktest_run");
    for (; *line != L'\0'; line++) {
        if (line[0] == L' ' && line[1] == L't' && line[2] == L'd' && line[3] == L'r')
            return av::GuestProgram::Run::timeout;
        if (line[0] == L' ' && line[1] == L'r' && line[2] == L'v' && line[3] == L'k')
            return av::GuestProgram::Run::rvk;
    }
    return av::GuestProgram::Run::suite;
}

/* rvktest's trace root: the one piece of process-lifetime state this program
 * has, since its unhandled-exception filter is handed no context
 * (constitution III). */
static constinit av::ObjectStorage<av::ProcessTrace> rvktest_trace;

static long WINAPI rvktest_crashed(void *pointers)
{
    AV_SCOPE("rvktest_crashed");
    return rvktest_trace.object().crashed(pointers);
}

extern "C" void rvktest_entry()
{
    const wchar_t *const line = GetCommandLineW();
    ExitProcess(av::GuestProgram(rvktest_stress_seconds(line), rvktest_run(line), rvktest_trace, &rvktest_crashed).run());
}

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" int _fltused = 0;
