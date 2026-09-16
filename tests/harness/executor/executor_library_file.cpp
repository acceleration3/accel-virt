#include "harness/executor/executor_library_file.hpp"

#include <dlfcn.h>

namespace av {

ExecutorLibraryFile::ExecutorLibraryFile(const char *path)
    : library_(dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE)), ops_(nullptr)
{
    if (library_ == nullptr) return;
    auto *const answer = reinterpret_cast<RvkExecutorOpsFunction>(dlsym(library_, RVK_EXECUTOR_OPS_SYMBOL));
    ops_ = answer != nullptr ? answer() : nullptr;
}

ExecutorLibraryFile::~ExecutorLibraryFile()
{
    if (library_ != nullptr) dlclose(library_);
}

const RvkExecutorOps *ExecutorLibraryFile::ops() const { return ops_; }

}  // namespace av
