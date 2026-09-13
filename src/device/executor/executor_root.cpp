#include "device/executor/executor_root.hpp"

#include "device/executor/host_apis.hpp"
#include "device/executor/host_gpu.hpp"

namespace av {

ExecutorRoot::ExecutorRoot() : gpus_(), asked_(false) {}

Status ExecutorRoot::initialize(HostApis &apis)
{
    if (asked_) return Status::invalid_state;
    const Status status = gpus_.initialize(apis);
    /* A host with more GPUs than the set holds is a host this still serves,
     * with the ones it kept. Anything else is a failure to answer at all. */
    if (status != Status::ok && status != Status::no_space) return status;
    asked_ = true;
    return Status::ok;
}

uint32_t ExecutorRoot::sub_apis() const { return asked_ ? gpus_.sub_apis() : 0u; }

uint32_t ExecutorRoot::import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const
{
    const HostGpu *const gpu = asked_ ? gpus_.find(gpu_uuid) : nullptr;
    return gpu != nullptr ? gpu->import_kinds(sub_api) : 0u;
}

bool ExecutorRoot::invariants_hold() const
{
    return gpus_.invariants_hold() && (asked_ || gpus_.count() == 0u);
}

}  // namespace av
