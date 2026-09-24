#include "um/trace/event_file.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

EventFile::EventFile() : region_(nullptr), bytes_(0u)
{
    AV_SCOPE("EventFile::EventFile");
}

bool EventFile::wanted() const
{
    AV_SCOPE("EventFile::wanted");
    return GetFileAttributesW(L"C:\\ProgramData\\rvk\\perfetto") != INVALID_FILE_ATTRIBUTES;
}

uint32_t EventFile::events_per_slot() const
{
    AV_SCOPE("EventFile::events_per_slot");
    constexpr uint32_t fallback = 1u << 20u;
    constexpr uint32_t smallest = 1u << 16u;
    constexpr uint32_t largest = 1u << 24u;
    const HANDLE file = CreateFileW(L"C:\\ProgramData\\rvk\\perfetto", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fallback;
    char  text[16] = {};
    DWORD read = 0u;
    const BOOL got = ReadFile(file, text, sizeof text - 1u, &read, nullptr);
    CloseHandle(file);
    uint32_t value = 0u;
    for (DWORD index = 0u; got && index < read && text[index] >= '0' && text[index] <= '9'; index++)
        value = value * 10u + static_cast<uint32_t>(text[index] - '0');
    if (value == 0u) return fallback;
    return value < smallest ? smallest : value > largest ? largest : value;
}

uint32_t EventFile::depth() const
{
    AV_SCOPE("EventFile::depth");
    constexpr uint32_t fallback = 2u;
    const HANDLE file = CreateFileW(L"C:\\ProgramData\\rvk\\perfetto", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fallback;
    char  text[32] = {};
    DWORD read = 0u;
    const BOOL got = ReadFile(file, text, sizeof text - 1u, &read, nullptr);
    CloseHandle(file);
    DWORD index = 0u;
    while (got && index < read && text[index] >= '0' && text[index] <= '9') index++;
    while (got && index < read && text[index] == ' ') index++;
    if (!got || index >= read || text[index] < '0' || text[index] > '9') return fallback;
    uint32_t value = 0u;
    for (; index < read && text[index] >= '0' && text[index] <= '9'; index++)
        value = value * 10u + static_cast<uint32_t>(text[index] - '0');
    return value;
}

/* The mapping and the file handle are left open for the process's life: the
 * table writes into the view until the process ends. */
Status EventFile::open(const char *module, uint32_t process, size_t bytes)
{
    AV_SCOPE("EventFile::open");
    if (region_ != nullptr) return Status::invalid_state;
    char       narrow[96] = {};
    TextBuffer name(narrow, sizeof narrow);
    name.append("C:\\ProgramData\\rvk\\events-");
    name.append(module);
    name.append("-");
    name.append_decimal(process);
    name.append(".bin");
    wchar_t wide[96] = {};
    for (size_t index = 0u; index + 1u < sizeof narrow && narrow[index] != '\0'; index++) wide[index] = static_cast<wchar_t>(static_cast<unsigned char>(narrow[index]));
    const HANDLE file = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Status::no_memory;
    /* Sparse: the rings of threads that never run take no disk. */
    /* FSCTL_SET_SPARSE: CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED,
     * FILE_SPECIAL_ACCESS), which the user-mode kit here has no winioctl.h for. */
    constexpr DWORD set_sparse = 0x000900C4u;
    DWORD returned = 0u;
    static_cast<void>(DeviceIoControl(file, set_sparse, nullptr, 0u, nullptr, 0u, &returned, nullptr));
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, static_cast<DWORD>(bytes >> 32u),
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
