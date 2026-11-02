#include "harness/executor/host_gpu_set_probe.hpp"

namespace av {

HostGpuSetProbe::HostGpuSetProbe(HostGpuSet &target) : set(target) {}

HostGpu &HostGpuSetProbe::gpu_at(uint32_t index) { return set.gpus_[index]; }

void HostGpuSetProbe::set_count(uint32_t count) { set.count_ = count; }

}
