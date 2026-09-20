/* An executor library opened by path, the way QEMU's glue opens one: its
 * symbols kept to itself, and the table its rvk_executor_ops answers. This is
 * what lets `rvkrecord replay` drive the same library the device drives. */
#ifndef AV_POSIX_EXECUTOR_MODULE_HPP
#define AV_POSIX_EXECUTOR_MODULE_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"

namespace av {

class PosixExecutorModule {
public:
    constexpr PosixExecutorModule() = default;
    ~PosixExecutorModule();
    PosixExecutorModule(const PosixExecutorModule &) = delete;
    PosixExecutorModule &operator=(const PosixExecutorModule &) = delete;

    /* not_found for a library that does not open, not_supported for one that
     * exports no rvk_executor_ops or answers no table, invalid_state once
     * open. */
    [[nodiscard]] Status  open(const char *path);
    const RvkExecutorOps *ops() const;

private:
    void                 *library_ = nullptr;
    const RvkExecutorOps *ops_ = nullptr;
};

}  // namespace av

#endif  // AV_POSIX_EXECUTOR_MODULE_HPP
