#include "device/executor/host_gpu_set.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/host_apis.hpp"

namespace av {

HostGpuSet::HostGpuSet() : gpus_{}, count_(0u), overflowed_(false)
{
    AV_SCOPE("HostGpuSet::HostGpuSet");
}

void HostGpuSet::initialize(HostApis &apis)
{
    AV_SCOPE("HostGpuSet::initialize");
    const uint32_t found = apis.gpu_count();
    const uint32_t taken = found < max_gpus ? found : max_gpus;
    for (uint32_t i = 0u; i < taken; i++) gpus_[i].describe(apis, i);
    count_ = taken;
    overflowed_ = found > max_gpus;
}

bool HostGpuSet::overflowed() const
{
    AV_SCOPE("HostGpuSet::overflowed");
    return overflowed_;
}

uint32_t HostGpuSet::count() const
{
    AV_SCOPE("HostGpuSet::count");
    return count_;
}

const HostGpu *HostGpuSet::find(const uint8_t *uuid) const
{
    AV_SCOPE("HostGpuSet::find");
    for (uint32_t i = 0u; i < count_; i++) {
        if (gpus_[i].matches(uuid)) return &gpus_[i];
    }
    return nullptr;
}

uint32_t HostGpuSet::sub_apis() const
{
    AV_SCOPE("HostGpuSet::sub_apis");
    uint32_t offered = 0u;
    for (uint32_t i = 0u; i < count_; i++) offered |= gpus_[i].sub_apis();
    return offered;
}

bool HostGpuSet::invariants_hold() const
{
    AV_SCOPE("HostGpuSet::invariants_hold");
    if (count_ > max_gpus) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (!gpus_[i].invariants_hold()) return false;
    }
    return true;
}

}  // namespace av
