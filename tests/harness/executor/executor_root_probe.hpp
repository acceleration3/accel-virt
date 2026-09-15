/* Reaches into an ExecutorRoot so invariant tests can corrupt it, or the GPUs
 * it found. Declared a friend by ExecutorRoot; nothing outside the tests uses
 * it. */
#ifndef AV_EXECUTOR_ROOT_PROBE_HPP
#define AV_EXECUTOR_ROOT_PROBE_HPP

#include "device/executor/executor_root.hpp"

#include <stdint.h>

namespace av {

class ContextSession;
class HostGpuSet;

struct ExecutorRootProbe {
    explicit ExecutorRootProbe(ExecutorRoot &target);

    HostGpuSet &gpus();
    void        forget_host();
    ContextSession &session_at(uint32_t slot);

    ExecutorRoot &root;
};

}  // namespace av

#endif  // AV_EXECUTOR_ROOT_PROBE_HPP
