#include "core/trace/function_scope.hpp"
#include "um/probe/gl/gl_discovery_probe.hpp"
#include "um/probe/gl/gl_probe_program.hpp"

#include <windows.h>

static bool avglprobe_discovery(const wchar_t *line)
{
    AV_SCOPE("avglprobe_discovery");
    constexpr wchar_t flag[] = L"--discovery";
    for (; *line != L'\0'; line++) {
        size_t matched = 0u;
        while (flag[matched] != L'\0' && line[matched] == flag[matched]) matched++;
        if (flag[matched] == L'\0') return true;
    }
    return false;
}

extern "C" void avglprobe_entry()
{
    ExitProcess(avglprobe_discovery(GetCommandLineW()) ? av::GlDiscoveryProbe().run() : av::GlProbeProgram().run());
}
