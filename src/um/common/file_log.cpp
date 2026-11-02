#include "um/common/file_log.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t suffix[] = L".log";
constexpr size_t stem_chars = 32u;
constexpr size_t path_chars =
    DiagnosticsDirectory::prefix_chars + stem_chars + 1u + 10u + 1u + 10u + sizeof suffix / sizeof suffix[0];
constexpr size_t line_bytes = 256u;

}

FileLog::FileLog(const wchar_t *stem) : directory_(), stem_(stem), file_(INVALID_HANDLE_VALUE)
{
    AV_SCOPE("FileLog::FileLog");
}

FileLog::~FileLog()
{
    AV_SCOPE("FileLog::~FileLog");
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
}

void FileLog::open()
{
    AV_SCOPE("FileLog::open");
    wchar_t path[path_chars] = {};
    size_t  length = directory_.place(L"", path, path_chars);
    for (size_t index = 0u; stem_[index] != L'\0' && index < stem_chars; index++) path[length++] = stem_[index];
    path[length++] = L'-';
    wchar_t digits[10] = {};
    size_t  count = 0u;
    for (DWORD pid = GetCurrentProcessId(); count == 0u || pid != 0u; pid /= 10u)
        digits[count++] = static_cast<wchar_t>(L'0' + pid % 10u);
    while (count != 0u) path[length++] = digits[--count];
    for (const wchar_t *text = suffix; *text != L'\0'; text++) path[length++] = *text;

    directory_.make();
    file_ = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ != INVALID_HANDLE_VALUE) return;
    length -= sizeof suffix / sizeof suffix[0] - 1u;
    path[length++] = L'-';
    count = 0u;
    for (ULONGLONG tick = GetTickCount64(); count == 0u || (tick != 0u && count < 10u); tick /= 10u)
        digits[count++] = static_cast<wchar_t>(L'0' + tick % 10u);
    while (count != 0u) path[length++] = digits[--count];
    for (const wchar_t *text = suffix; *text != L'\0'; text++) path[length++] = *text;
    path[length] = L'\0';
    file_ = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
}

void FileLog::write(const char *line)
{
    AV_SCOPE("FileLog::write");
    if (file_ == INVALID_HANDLE_VALUE) return;
    char   bytes[line_bytes] = {};
    size_t length = 0u;
    for (; line[length] != '\0' && length + 2u < sizeof bytes; length++) bytes[length] = line[length];
    bytes[length++] = '\r';
    bytes[length++] = '\n';
    DWORD written = 0u;
    static_cast<void>(WriteFile(file_, bytes, static_cast<DWORD>(length), &written, nullptr));
}

}
