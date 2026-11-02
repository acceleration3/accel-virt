#ifndef AV_MODULE_RESOLVER_HPP
#define AV_MODULE_RESOLVER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ModuleResolver {
public:
    ModuleResolver() = default;
    ModuleResolver(const ModuleResolver &) = delete;
    ModuleResolver &operator=(const ModuleResolver &) = delete;

    virtual bool resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset) = 0;

protected:
    ~ModuleResolver() = default;
};

}

#endif
