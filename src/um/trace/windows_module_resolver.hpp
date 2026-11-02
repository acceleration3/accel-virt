#ifndef AV_UM_WINDOWS_MODULE_RESOLVER_HPP
#define AV_UM_WINDOWS_MODULE_RESOLVER_HPP

#include "core/trace/module_resolver.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowsModuleResolver final : public ModuleResolver {
public:
    WindowsModuleResolver();

    bool resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset) override;
    bool image_of(uint64_t address, uint64_t &base, uint64_t &size);

private:
    static constexpr uint32_t path_chars = 520u;

    wchar_t path_[path_chars];
};

}

#endif
