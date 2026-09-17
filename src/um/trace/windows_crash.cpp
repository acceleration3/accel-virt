#include "um/trace/windows_crash.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/crash_event.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/trace_host.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "core/trace/trace_text.hpp"
#include "um/trace/wide_name.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t directory[] = L"C:\\ProgramData\\rvk";
constexpr size_t  line_bytes = 512u;

}  // namespace

WindowsCrash::WindowsCrash() : modules_(), returns_{}
{
    AV_SCOPE("WindowsCrash::WindowsCrash");
}

void WindowsCrash::write(const char *module, TraceTable &table, TraceHost &host, void *pointers, uint64_t anchor)
{
    AV_SCOPE("WindowsCrash::write");
    const auto *const exception = static_cast<EXCEPTION_POINTERS *>(pointers)->ExceptionRecord;
    const uint64_t    address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(exception->ExceptionAddress));
    table.record_crash(exception->ExceptionCode, address);
    const uint32_t frames = unwind(pointers);

    TraceSnapshot snapshot;
    if (snapshot.read(table.region(), table.bytes()) != Status::ok) return;
    uint64_t base = 0u;
    uint64_t size = 0u;
    if (!modules_.image_of(anchor, base, size)) size = 0u;
    TraceText         text(snapshot, ImageRange(base, size), host.clock());
    const CrashEvent  event(module, GetCurrentProcessId(), exception->ExceptionCode, address);
    CrashReport       report(event, text, modules_, returns_, frames);

    static_cast<void>(CreateDirectoryW(directory, nullptr));
    WideName path;
    path.append("C:\\ProgramData\\rvk\\crash-");
    path.append(module);
    path.append("-");
    path.append(static_cast<uint32_t>(GetCurrentProcessId()));
    path.append(".txt");
    HANDLE file = CreateFileW(path.text(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    for (;;) {
        char       bytes[line_bytes];
        TextBuffer line(bytes, sizeof bytes - 2u);
        if (!report.next(line)) break;
        size_t used = line.length();
        bytes[used++] = '\r';
        bytes[used++] = '\n';
        DWORD written = 0u;
        static_cast<void>(WriteFile(file, bytes, static_cast<DWORD>(used), &written, nullptr));
    }
    CloseHandle(file);
}

uint32_t WindowsCrash::unwind(void *pointers)
{
    AV_SCOPE("WindowsCrash::unwind");
    CONTEXT context = *static_cast<EXCEPTION_POINTERS *>(pointers)->ContextRecord;
    uint32_t count = 0u;
    while (count < CrashReport::max_frames && context.Rip != 0u) {
        returns_[count++] = context.Rip;
        DWORD64            image_base = 0u;
        PRUNTIME_FUNCTION  function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
        if (function == nullptr) {
            /* A leaf function: its return address is on top of the stack. */
            context.Rip = *reinterpret_cast<DWORD64 *>(context.Rsp);
            context.Rsp += sizeof(DWORD64);
            continue;
        }
        PVOID   handler_data = nullptr;
        DWORD64 establisher = 0u;
        static_cast<void>(RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context,
                                           &handler_data, &establisher, nullptr));
    }
    return count;
}

}  // namespace av
