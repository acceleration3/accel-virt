#include "harness/executor/host_gpu_probe.hpp"

namespace av {

HostGpuProbe::HostGpuProbe(HostGpu &target) : gpu(target) {}

void HostGpuProbe::set_sub_apis(uint32_t sub_apis) { gpu.sub_apis_ = sub_apis; }

}  // namespace av
