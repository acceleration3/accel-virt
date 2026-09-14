#include "harness/executor/executor_root_probe.hpp"

namespace av {

ExecutorRootProbe::ExecutorRootProbe(ExecutorRoot &target) : root(target) {}

HostGpuSet &ExecutorRootProbe::gpus() { return root.gpus_; }

void ExecutorRootProbe::forget_host() { root.apis_ = nullptr; }

}  // namespace av
