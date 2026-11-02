#ifndef AV_POSIX_EXECUTOR_MODULE_HPP
#define AV_POSIX_EXECUTOR_MODULE_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"

namespace av {

class PosixExecutorModule {
public:
    constexpr PosixExecutorModule() = default;
    ~PosixExecutorModule();
    PosixExecutorModule(const PosixExecutorModule &) = delete;
    PosixExecutorModule &operator=(const PosixExecutorModule &) = delete;

    [[nodiscard]] Status  open(const char *path);
    const AvExecutorOps *ops() const;

private:
    void                 *library_ = nullptr;
    const AvExecutorOps *ops_ = nullptr;
};

}

#endif
