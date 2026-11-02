#include "um/probe/windows_image_sink.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t directory[] = L"C:\\av\\";
constexpr wchar_t extension[] = L".bmp";
constexpr size_t  path_chars = sizeof directory / sizeof directory[0] + ImageSink::max_name +
                              sizeof extension / sizeof extension[0];

}

WindowsImageSink::WindowsImageSink() : bytes_{}
{
    AV_SCOPE("WindowsImageSink::WindowsImageSink");
}

void WindowsImageSink::save(const char *name, const BmpImage &image)
{
    AV_SCOPE("WindowsImageSink::save");
    if (image.encode(bytes_, sizeof bytes_) != Status::ok) return;
    wchar_t path[path_chars] = {};
    size_t  length = 0u;
    for (const wchar_t *text = directory; *text != L'\0'; text++) path[length++] = *text;
    for (size_t at = 0u; name[at] != '\0' && at + 1u < max_name; at++) path[length++] = static_cast<wchar_t>(name[at]);
    for (const wchar_t *text = extension; *text != L'\0'; text++) path[length++] = *text;
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0u;
    static_cast<void>(WriteFile(file, bytes_, static_cast<DWORD>(image.encoded_size()), &written, nullptr));
    static_cast<void>(CloseHandle(file));
}

}
