#include "core/trace/function_scope.hpp"
#include "um/avtest/guest_program.hpp"

#include <windows.h>

static uint32_t avtest_stress_seconds(const wchar_t *line)
{
    AV_SCOPE("avtest_stress_seconds");
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

static av::GuestProgram::Run avtest_run(const wchar_t *line)
{
    AV_SCOPE("avtest_run");
    for (; *line != L'\0'; line++) {
        if (line[0] == L' ' && line[1] == L't' && line[2] == L'd' && line[3] == L'r')
            return av::GuestProgram::Run::timeout;
        if (line[0] == L' ' && line[1] == L'a' && line[2] == L'v' &&
            (line[3] == L'\0' || line[3] == L' '))
            return av::GuestProgram::Run::av;
        if (line[0] == L' ' && line[1] == L't' && line[2] == L'r' && line[3] == L'i' && line[4] == L'p')
            return av::GuestProgram::Run::round_trip;
        if (line[0] == L' ' && line[1] == L'l' && line[2] == L'o' && line[3] == L's' && line[4] == L's')
            return av::GuestProgram::Run::host_loss;
    }
    return av::GuestProgram::Run::suite;
}

static constinit av::ObjectStorage<av::ProcessTrace> avtest_trace;

static long WINAPI avtest_crashed(void *pointers)
{
    AV_SCOPE("avtest_crashed");
    return avtest_trace.object().crashed(pointers);
}

extern "C" void avtest_entry()
{
    const wchar_t *const line = GetCommandLineW();
    ExitProcess(av::GuestProgram(avtest_stress_seconds(line), avtest_run(line), avtest_trace, &avtest_crashed).run());
}
