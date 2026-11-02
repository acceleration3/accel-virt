#include "um/common/diagnostics_directory.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t root[] = L"C:\\ProgramData\\av";
static_assert(sizeof root / sizeof root[0] == DiagnosticsDirectory::prefix_chars,
              "the prefix is the directory and its separator");

}

DiagnosticsDirectory::DiagnosticsDirectory() : root_(root)
{
    AV_SCOPE("DiagnosticsDirectory::DiagnosticsDirectory");
}

void DiagnosticsDirectory::make() const
{
    AV_SCOPE("DiagnosticsDirectory::make");
    static_cast<void>(CreateDirectoryW(root_, nullptr));
}

bool DiagnosticsDirectory::flagged(const wchar_t *name) const
{
    AV_SCOPE("DiagnosticsDirectory::flagged");
    wchar_t path[MAX_PATH] = {};
    return place(name, path, MAX_PATH) != 0u && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

size_t DiagnosticsDirectory::place(const wchar_t *name, wchar_t *path, size_t capacity) const
{
    AV_SCOPE("DiagnosticsDirectory::place");
    size_t length = 0u;
    for (const wchar_t *text = root_; *text != L'\0'; text++) {
        if (length + 1u >= capacity) return 0u;
        path[length++] = *text;
    }
    if (length + 1u >= capacity) return 0u;
    path[length++] = L'\\';
    for (const wchar_t *text = name; *text != L'\0'; text++) {
        if (length + 1u >= capacity) return 0u;
        path[length++] = *text;
    }
    path[length] = L'\0';
    return length;
}

}
