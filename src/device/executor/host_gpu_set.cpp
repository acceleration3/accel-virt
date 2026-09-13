#include "device/executor/host_gpu_set.hpp"

#include "device/executor/host_apis.hpp"

namespace av {

HostGpuSet::HostGpuSet() : gpus_{}, count_(0u) {}

Status HostGpuSet::initialize(HostApis &apis)
{
    const uint32_t found = apis.gpu_count();
    const uint32_t taken = found < max_gpus ? found : max_gpus;
    for (uint32_t i = 0u; i < taken; i++) {
        const Status status = gpus_[i].initialize(apis, i);
        if (status != Status::ok) return status;
        count_ = i + 1u;
    }
    return found > max_gpus ? Status::no_space : Status::ok;
}

uint32_t HostGpuSet::count() const { return count_; }

const HostGpu *HostGpuSet::find(const uint8_t *uuid) const
{
    for (uint32_t i = 0u; i < count_; i++) {
        if (gpus_[i].matches(uuid)) return &gpus_[i];
    }
    return nullptr;
}

uint32_t HostGpuSet::sub_apis() const
{
    uint32_t offered = 0u;
    for (uint32_t i = 0u; i < count_; i++) offered |= gpus_[i].sub_apis();
    return offered;
}

bool HostGpuSet::invariants_hold() const
{
    if (count_ > max_gpus) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (!gpus_[i].invariants_hold()) return false;
    }
    return true;
}

}  // namespace av
