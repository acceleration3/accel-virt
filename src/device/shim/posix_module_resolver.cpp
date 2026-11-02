#include "device/shim/posix_module_resolver.hpp"

#include "core/trace/function_scope.hpp"

#include <dlfcn.h>
#include <errno.h>
#include <link.h>

namespace av {

PosixModuleResolver::PosixModuleResolver() : program_(program_invocation_short_name)
{
    AV_SCOPE("PosixModuleResolver::PosixModuleResolver");
}

bool PosixModuleResolver::resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset)
{
    AV_SCOPE("PosixModuleResolver::resolve");
    Dl_info  found = {};
    void    *linked = nullptr;
    if (capacity == 0u ||
        dladdr1(reinterpret_cast<void *>(static_cast<uintptr_t>(address)), &found, &linked, RTLD_DL_LINKMAP) == 0 ||
        linked == nullptr)
        return false;
    const auto *const object = static_cast<const link_map *>(linked);
    const char *path = found.dli_fname != nullptr && found.dli_fname[0] != '\0' ? found.dli_fname : program_;
    const char *base = path;
    for (const char *cursor = path; *cursor != '\0'; ++cursor)
        if (*cursor == '/') base = cursor + 1;
    size_t length = 0u;
    for (; base[length] != '\0' && length + 1u < capacity; length++) name[length] = base[length];
    name[length] = '\0';
    offset = address - static_cast<uint64_t>(object->l_addr);
    return true;
}

}
