#include "harness/executor/executor_root_probe.hpp"

namespace av {

ExecutorRootProbe::ExecutorRootProbe(ExecutorRoot &target) : root(target) {}

HostGpuSet &ExecutorRootProbe::gpus() { return root.gpus_; }

void ExecutorRootProbe::forget_host() { root.apis_ = nullptr; }

ContextSession &ExecutorRootProbe::session_at(uint32_t slot) { return root.sessions_[slot]; }

}  // namespace av
