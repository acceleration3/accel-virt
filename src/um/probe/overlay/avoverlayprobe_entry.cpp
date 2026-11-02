#include "um/probe/overlay/overlay_probe_program.hpp"

#include "core/trace/function_scope.hpp"

#include <stdint.h>
#include <windows.h>

extern "C" void avoverlayprobe_entry();

namespace {
uint32_t hold_presents()
{
    AV_SCOPE("hold_presents");
    const wchar_t *line = GetCommandLineW();
    const wchar_t *word = line;
    for (const wchar_t *at = line; *at != L'\0'; at++)
        if (*at == L' ' && at[1] != L' ' && at[1] != L'\0') word = at + 1;
    uint32_t value = 0u;
    for (; *word >= L'0' && *word <= L'9' && value < 100000u; word++) value = value * 10u + static_cast<uint32_t>(*word - L'0');
    return *word == L'\0' || *word == L' ' ? value : 0u;
}

}

extern "C" void avoverlayprobe_entry() { ExitProcess(av::OverlayProbeProgram(hold_presents()).run()); }
