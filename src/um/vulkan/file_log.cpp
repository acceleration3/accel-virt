#include "um/vulkan/file_log.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t directory[] = L"C:\\ProgramData\\rvk";
constexpr wchar_t prefix[] = L"C:\\ProgramData\\rvk\\rvkvk-";
constexpr wchar_t suffix[] = L".log";
/* The prefix, ten digits of a DWORD, the suffix and the NUL. */
constexpr size_t path_chars = sizeof prefix / sizeof prefix[0] + 10u + sizeof suffix / sizeof suffix[0];
constexpr size_t line_bytes = 256u;

}  // namespace

FileLog::FileLog() : file_(INVALID_HANDLE_VALUE) {}

FileLog::~FileLog()
{
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
}

void FileLog::open()
{
    wchar_t path[path_chars] = {};
    size_t  length = 0u;
    for (const wchar_t *text = prefix; *text != L'\0'; text++) path[length++] = *text;
    wchar_t digits[10] = {};
    size_t  count = 0u;
    for (DWORD pid = GetCurrentProcessId(); count == 0u || pid != 0u; pid /= 10u)
        digits[count++] = static_cast<wchar_t>(L'0' + pid % 10u);
    while (count != 0u) path[length++] = digits[--count];
    for (const wchar_t *text = suffix; *text != L'\0'; text++) path[length++] = *text;

    static_cast<void>(CreateDirectoryW(directory, nullptr));
    file_ = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
}

void FileLog::write(const char *line)
{
    if (file_ == INVALID_HANDLE_VALUE) return;
    char   bytes[line_bytes] = {};
    size_t length = 0u;
    for (; line[length] != '\0' && length + 2u < sizeof bytes; length++) bytes[length] = line[length];
    bytes[length++] = '\r';
    bytes[length++] = '\n';
    DWORD written = 0u;
    static_cast<void>(WriteFile(file_, bytes, static_cast<DWORD>(length), &written, nullptr));
}

}  // namespace av
