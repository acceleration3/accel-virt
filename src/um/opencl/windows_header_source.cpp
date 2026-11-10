#include "um/opencl/windows_header_source.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

WindowsHeaderSource::WindowsHeaderSource() : reads_(0u)
{
    AV_SCOPE("WindowsHeaderSource::WindowsHeaderSource");
}

uint32_t WindowsHeaderSource::reads() const
{
    AV_SCOPE("WindowsHeaderSource::reads");
    return reads_;
}

bool WindowsHeaderSource::fetch(BlockSource &memory, const char *directory, const char *name, char *&text,
                                size_t &bytes)
{
    AV_SCOPE("WindowsHeaderSource::fetch");
    text = nullptr;
    bytes = 0u;
    char   path[MAX_PATH] = {};
    size_t at = 0u;
    for (size_t index = 0u; directory[index] != '\0' && at + 2u < sizeof path; ++index) path[at++] = directory[index];
    if (at != 0u && path[at - 1u] != '\\' && path[at - 1u] != '/') path[at++] = '\\';
    for (size_t index = 0u; name[index] != '\0'; ++index) {
        if (at + 1u >= sizeof path) return false;
        path[at++] = name[index] == '/' ? '\\' : name[index];
    }
    path[at] = '\0';
    HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD high = 0u;
    const DWORD low = GetFileSize(file, &high);
    bool        read = false;
    if (low != INVALID_FILE_SIZE && high == 0u && low != 0u && low < (16u << 20u)) {
        char *const block = static_cast<char *>(memory.allocate(static_cast<size_t>(low) + 1u, Pool::paged));
        DWORD       got = 0u;
        if (block != nullptr && ReadFile(file, block, low, &got, nullptr) != 0 && got == low) {
            block[got] = '\0';
            text = block;
            bytes = got;
            read = true;
            ++reads_;
        } else if (block != nullptr) {
            memory.release(block);
        }
    }
    CloseHandle(file);
    return read;
}

}
