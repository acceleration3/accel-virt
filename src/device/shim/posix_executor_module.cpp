#include "device/shim/posix_executor_module.hpp"

#include "core/trace/function_scope.hpp"

#include <dlfcn.h>

namespace av {
PosixExecutorModule::~PosixExecutorModule()
{
    AV_SCOPE("PosixExecutorModule::~PosixExecutorModule");
    if (library_ != nullptr) dlclose(library_);
}

Status PosixExecutorModule::open(const char *path)
{
    AV_SCOPE("PosixExecutorModule::open");
    if (library_ != nullptr) return Status::invalid_state;
    void *const library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) return Status::not_found;
    library_ = library;
    void *const symbol = dlsym(library, AV_EXECUTOR_OPS_SYMBOL);
    if (symbol == nullptr) return Status::not_supported;
    AvExecutorOpsFunction answer = nullptr;
    __builtin_memcpy(&answer, &symbol, sizeof answer);
    ops_ = answer();
    return ops_ != nullptr ? Status::ok : Status::not_supported;
}

const AvExecutorOps *PosixExecutorModule::ops() const
{
    AV_SCOPE("PosixExecutorModule::ops");
    return ops_;
}

}
