#ifndef AV_EXECUTOR_ROOT_PROBE_HPP
#define AV_EXECUTOR_ROOT_PROBE_HPP

#include "executor/executor_root.hpp"

#include <stdint.h>

namespace av {

class ContextSession;
class HostGpuSet;
class SessionShares;

struct ExecutorRootProbe {
    explicit ExecutorRootProbe(ExecutorRoot &target);

    HostGpuSet &gpus();
    void        forget_host();
    ContextSession &session_at(uint32_t slot);
    SessionShares  &shares_at(uint32_t slot);
    void            remember(uint32_t slot, uint32_t sub_api, uint32_t gpu);

    ExecutorRoot &root;
};

}

#endif
