#ifndef AV_POSIX_MODULE_RESOLVER_HPP
#define AV_POSIX_MODULE_RESOLVER_HPP

#include "core/trace/module_resolver.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PosixModuleResolver final : public ModuleResolver {
public:
    PosixModuleResolver();

    bool resolve(uint64_t address, char *name, size_t capacity, uint64_t &offset) override;

private:
    const char *program_;
};

}

#endif
