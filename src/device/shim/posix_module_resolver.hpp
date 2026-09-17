/* Which loaded object an address lies in, from the dynamic linker, for a crash
 * file's native stack. dladdr is not on POSIX's async-signal-safe list; glibc's
 * takes the loader lock only for a moment, and a crash file is the last thing
 * the process writes either way. */
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
    /* The main program's name: dladdr names it with an empty path. */
    const char *program_;
};

}  // namespace av

#endif  // AV_POSIX_MODULE_RESOLVER_HPP
