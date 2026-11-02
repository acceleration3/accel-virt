#include "um/trace/windows_module_resolver.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

WindowsModuleResolver::WindowsModuleResolver() : path_{}
{
    AV_SCOPE("WindowsModuleResolver::WindowsModuleResolver");
}

bool WindowsModuleResolver::resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset)
{
    AV_SCOPE("WindowsModuleResolver::resolve");
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (capacity == 0u || !GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(static_cast<uintptr_t>(address)), &module))
        return false;
    const DWORD length = GetModuleFileNameW(module, path_, path_chars);
    DWORD       start = 0u;
    for (DWORD index = 0u; index < length; index++)
        if (path_[index] == L'\\') start = index + 1u;
    size_t written = 0u;
    for (DWORD index = start; index < length && written + 1u < capacity; index++)
        name[written++] = path_[index] < 0x80 ? static_cast<char>(path_[index]) : '?';
    name[written] = '\0';
    offset = address - static_cast<uint64_t>(reinterpret_cast<uintptr_t>(module));
    return true;
}

bool WindowsModuleResolver::image_of(uint64_t address, uint64_t &base, uint64_t &size)
{
    AV_SCOPE("WindowsModuleResolver::image_of");
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(static_cast<uintptr_t>(address)), &module)) return false;
    const auto *const dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
    const auto *const nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(reinterpret_cast<const BYTE *>(module) + dos->e_lfanew);
    base = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(module));
    size = nt->OptionalHeader.SizeOfImage;
    return true;
}

}
