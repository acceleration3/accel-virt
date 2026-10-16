#include "um/trace/event_file.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/trace_table.hpp"

#include <windows.h>

namespace av {

EventFile::EventFile() : directory_(), region_(nullptr), bytes_(0u)
{
    AV_SCOPE("EventFile::EventFile");
}

bool EventFile::wanted() const
{
    AV_SCOPE("EventFile::wanted");
    return directory_.flagged(L"perfetto");
}

/* The flag file's text, "<events per slot> <depth> <scope,scope,...>", every
 * field optional; empty when there is none. */
void EventFile::read_flag(char *text, uint32_t capacity) const
{
    AV_SCOPE("EventFile::read_flag");
    text[0] = '\0';
    wchar_t path[MAX_PATH] = {};
    if (directory_.place(L"perfetto", path, MAX_PATH) == 0u) return;
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD read = 0u;
    if (!ReadFile(file, text, capacity - 1u, &read, nullptr)) read = 0u;
    CloseHandle(file);
    text[read] = '\0';
}

/* Where the flag's field `index` starts, or its end when it has fewer. */
const char *EventFile::field(const char *text, uint32_t index) const
{
    AV_SCOPE("EventFile::field");
    const char *at = text;
    while (*at == ' ') at++;
    for (; index != 0u && *at != '\0'; index--) {
        while (*at > ' ') at++;
        while (*at == ' ') at++;
    }
    return at;
}

uint32_t EventFile::number(uint32_t index, uint32_t fallback) const
{
    AV_SCOPE("EventFile::number");
    char text[flag_capacity] = {};
    read_flag(text, sizeof text);
    const char *at = field(text, index);
    if (*at < '0' || *at > '9') return fallback;
    uint32_t value = 0u;
    for (; *at >= '0' && *at <= '9'; at++) value = value * 10u + static_cast<uint32_t>(*at - '0');
    return value;
}

uint32_t EventFile::events_per_slot() const
{
    AV_SCOPE("EventFile::events_per_slot");
    constexpr uint32_t fallback = 1u << 20u;
    constexpr uint32_t smallest = 1u << 16u;
    constexpr uint32_t largest = 1u << 24u;
    const uint32_t     value = number(0u, fallback);
    if (value == 0u) return fallback;
    return value < smallest ? smallest : value > largest ? largest : value;
}

uint32_t EventFile::depth() const
{
    AV_SCOPE("EventFile::depth");
    constexpr uint32_t fallback = 2u;
    return number(1u, fallback);
}

void EventFile::only(TraceTable &table) const
{
    AV_SCOPE("EventFile::only");
    char text[flag_capacity] = {};
    read_flag(text, sizeof text);
    table.only(field(text, 2u));
}

/* The mapping and the file handle are left open for the process's life: the
 * table writes into the view until the process ends. */
Status EventFile::open(const char *module, uint32_t process, size_t bytes)
{
    AV_SCOPE("EventFile::open");
    if (region_ != nullptr) return Status::invalid_state;
    char       narrow[78] = {};
    TextBuffer name(narrow, sizeof narrow);
    name.append("events-");
    name.append(module);
    name.append("-");
    name.append_decimal(process);
    name.append(".bin");
    wchar_t widened[sizeof narrow] = {};
    for (size_t index = 0u; index + 1u < sizeof narrow && narrow[index] != '\0'; index++)
        widened[index] = static_cast<wchar_t>(static_cast<unsigned char>(narrow[index]));
    wchar_t wide[DiagnosticsDirectory::prefix_chars + sizeof narrow] = {};
    if (directory_.place(widened, wide, sizeof wide / sizeof wide[0]) == 0u) return Status::invalid_parameter;
    const HANDLE file = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Status::no_memory;
    /* Sparse: the rings of threads that never run take no disk. */
    /* FSCTL_SET_SPARSE: CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED,
     * FILE_SPECIAL_ACCESS), which the user-mode kit here has no winioctl.h for. */
    constexpr DWORD set_sparse = 0x000900C4u;
    DWORD returned = 0u;
    static_cast<void>(DeviceIoControl(file, set_sparse, nullptr, 0u, nullptr, 0u, &returned, nullptr));
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, static_cast<DWORD>(static_cast<uint64_t>(bytes) >> 32u),
                                              static_cast<DWORD>(bytes), nullptr);
    if (mapping == nullptr) return Status::no_memory;
    void *const view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0u, 0u, bytes);
    if (view == nullptr) return Status::no_memory;
    region_ = static_cast<uint8_t *>(view);
    bytes_ = bytes;
    return Status::ok;
}

uint8_t *EventFile::region() const
{
    AV_SCOPE("EventFile::region");
    return region_;
}

size_t EventFile::bytes() const
{
    AV_SCOPE("EventFile::bytes");
    return bytes_;
}

}  // namespace av
