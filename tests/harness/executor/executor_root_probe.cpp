#include "harness/executor/executor_root_probe.hpp"

namespace av {

ExecutorRootProbe::ExecutorRootProbe(ExecutorRoot &target) : root(target) {}

HostGpuSet &ExecutorRootProbe::gpus() { return root.gpus_; }

void ExecutorRootProbe::set_asked(bool asked) { root.asked_ = asked; }

}  // namespace av
