#include "um/inspect/inspect_session.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

InspectSession::InspectSession() : heap_(), kmt_(), host_(), program_(host_, kmt_, heap_)
{
    AV_SCOPE("InspectSession::InspectSession");
}

uint32_t InspectSession::run(const wchar_t *line)
{
    AV_SCOPE("InspectSession::run");
    static_assert(sizeof(wchar_t) == sizeof(uint16_t));
    const InspectProgram::Exit exit = program_.run(reinterpret_cast<const uint16_t *>(line));
    if (exit == InspectProgram::Exit::usage) {
        const char *const text = program_.usage();
        DWORD             length = 0u;
        while (text[length] != '\0') length++;
        DWORD written = 0u;
        static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, length, &written, nullptr));
    }
    return static_cast<uint32_t>(exit);
}

}  // namespace av
