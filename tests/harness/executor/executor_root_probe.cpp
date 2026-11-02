#include "harness/executor/executor_root_probe.hpp"

namespace av {

ExecutorRootProbe::ExecutorRootProbe(ExecutorRoot &target) : root(target) {}

HostGpuSet &ExecutorRootProbe::gpus() { return root.gpus_; }

void ExecutorRootProbe::forget_host() { root.apis_ = nullptr; }

ContextSession &ExecutorRootProbe::session_at(uint32_t slot) { return root.sessions_[slot]; }

SessionShares &ExecutorRootProbe::shares_at(uint32_t slot) { return root.shares_[slot]; }

void ExecutorRootProbe::remember(uint32_t slot, uint32_t sub_api, uint32_t gpu)
{
    root.apis_of_[slot] = sub_api;
    root.gpus_of_[slot] = gpu;
}

}
