/* ModuleResolver over the loader's own list: the module holding an address,
 * its file's name and the offset from its base. Safe in an exception filter:
 * no allocation, the reference count untouched. Translation only. */
#ifndef AV_UM_WINDOWS_MODULE_RESOLVER_HPP
#define AV_UM_WINDOWS_MODULE_RESOLVER_HPP

#include "guest/trace/module_resolver.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowsModuleResolver final : public ModuleResolver {
public:
    WindowsModuleResolver();

    bool resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset) override;
    /* The base and size of the module holding `address`; false outside one. */
    bool image_of(uint64_t address, uint64_t &base, uint64_t &size);

private:
    static constexpr uint32_t path_chars = 520u;

    wchar_t path_[path_chars];
};

}  // namespace av

#endif  // AV_UM_WINDOWS_MODULE_RESOLVER_HPP
