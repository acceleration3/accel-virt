#include "um/inspect/windows_inspect_host.hpp"

#include "core/trace/function_scope.hpp"
#include "um/trace/wide_name.hpp"

#include <windows.h>

extern "C" {
struct AvProcessEntry32W {
    DWORD     dwSize;
    DWORD     cntUsage;
    DWORD     th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD     th32ModuleID;
    DWORD     cntThreads;
    DWORD     th32ParentProcessID;
    LONG      pcPriClassBase;
    DWORD     dwFlags;
    WCHAR     szExeFile[MAX_PATH];
};
HANDLE WINAPI CreateToolhelp32Snapshot(DWORD flags, DWORD process);
BOOL WINAPI   Process32FirstW(HANDLE snapshot, AvProcessEntry32W *entry);
BOOL WINAPI   Process32NextW(HANDLE snapshot, AvProcessEntry32W *entry);
}

namespace av {

namespace {

constexpr DWORD path_chars = 520u;
constexpr DWORD snapshot_processes = 0x00000002u;
constexpr DWORD write_chunk = 1u << 20u;

}

WindowsInspectHost::WindowsInspectHost() : clock_()
{
    AV_SCOPE("WindowsInspectHost::WindowsInspectHost");
}

uint32_t WindowsInspectHost::processes(uint32_t *pids, uint32_t capacity)
{
    AV_SCOPE("WindowsInspectHost::processes");
    HANDLE snapshot = CreateToolhelp32Snapshot(snapshot_processes, 0u);
    if (snapshot == INVALID_HANDLE_VALUE) return 0u;
    AvProcessEntry32W entry = {};
    entry.dwSize = sizeof entry;
    uint32_t count = 0u;
    for (BOOL more = Process32FirstW(snapshot, &entry); more && count < capacity; more = Process32NextW(snapshot, &entry))
        pids[count++] = entry.th32ProcessID;
    CloseHandle(snapshot);
    return count;
}

Status WindowsInspectHost::executable(uint32_t pid, char *path, size_t capacity)
{
    AV_SCOPE("WindowsInspectHost::executable");
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr || capacity == 0u) return Status::not_found;
    wchar_t wide[path_chars] = {};
    DWORD   length = path_chars;
    const BOOL named = QueryFullProcessImageNameW(process, 0u, wide, &length);
    CloseHandle(process);
    if (!named) return Status::not_found;
    size_t written = 0u;
    for (DWORD index = 0u; index < length && written + 1u < capacity; index++)
        path[written++] = wide[index] < 0x80 ? static_cast<char>(wide[index]) : '?';
    path[written] = '\0';
    return Status::ok;
}

Status WindowsInspectHost::copy_section(const char *name, uint8_t *destination, size_t capacity, size_t &copied)
{
    AV_SCOPE("WindowsInspectHost::copy_section");
    WideName wide;
    wide.append(name);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, wide.text());
    if (mapping == nullptr) return Status::not_found;
    const void *const view = MapViewOfFile(mapping, FILE_MAP_READ, 0u, 0u, 0u);
    MEMORY_BASIC_INFORMATION region = {};
    const bool sized = view != nullptr && VirtualQuery(view, &region, sizeof region) == sizeof region;
    copied = 0u;
    if (sized) {
        const size_t available = region.RegionSize < capacity ? region.RegionSize : capacity;
        const auto  *source = static_cast<const uint8_t *>(view);
        for (; copied < available; copied++) destination[copied] = source[copied];
    }
    if (view != nullptr) UnmapViewOfFile(view);
    CloseHandle(mapping);
    return sized ? Status::ok : Status::not_found;
}

Status WindowsInspectHost::request_state(const char *dump, const char *done, uint32_t milliseconds)
{
    AV_SCOPE("WindowsInspectHost::request_state");
    WideName dump_name;
    WideName done_name;
    dump_name.append(dump);
    done_name.append(done);
    HANDLE ask = OpenEventW(EVENT_MODIFY_STATE, FALSE, dump_name.text());
    HANDLE answered = OpenEventW(SYNCHRONIZE, FALSE, done_name.text());
    Status status = Status::not_found;
    if (ask != nullptr && answered != nullptr && SetEvent(ask))
        status = WaitForSingleObject(answered, milliseconds) == WAIT_OBJECT_0 ? Status::ok : Status::invalid_state;
    if (ask != nullptr) CloseHandle(ask);
    if (answered != nullptr) CloseHandle(answered);
    return status;
}

Status WindowsInspectHost::write_file(const char *path, const uint8_t *head, size_t head_bytes, const uint8_t *body,
                                      size_t body_bytes)
{
    AV_SCOPE("WindowsInspectHost::write_file");
    WideName wide;
    wide.append(path);
    HANDLE file = CreateFileW(wide.text(), GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Status::not_found;
    bool                 whole = true;
    const uint8_t *const parts[] = {head, body};
    const size_t         sizes[] = {head_bytes, body_bytes};
    for (uint32_t part = 0u; part < 2u && whole; part++) {
        for (size_t offset = 0u; offset < sizes[part] && whole;) {
            const DWORD chunk = sizes[part] - offset < write_chunk ? static_cast<DWORD>(sizes[part] - offset) : write_chunk;
            DWORD       written = 0u;
            whole = WriteFile(file, parts[part] + offset, chunk, &written, nullptr) && written == chunk;
            offset += chunk;
        }
    }
    CloseHandle(file);
    return whole ? Status::ok : Status::no_space;
}

uint64_t WindowsInspectHost::clock() const
{
    AV_SCOPE("WindowsInspectHost::clock");
    return clock_.now();
}

}
