#include "device/executor/host_gpu.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/host_apis.hpp"

namespace av {

namespace {

/* The three sub-API bits, in the order their kinds are cached. */
constexpr uint32_t sub_api_bits[] = {RVK_EXECUTOR_SUB_API_VULKAN, RVK_EXECUTOR_SUB_API_OPENGL,
                                     RVK_EXECUTOR_SUB_API_OPENCL};

}  // namespace

HostGpu::HostGpu() : uuid_{}, sub_apis_(0u), kinds_{}, index_(0u), known_(false) {}

Status HostGpu::initialize(HostApis &apis, uint32_t index)
{
    if (index >= apis.gpu_count()) return Status::not_found;
    apis.gpu_uuid(index, uuid_);
    sub_apis_ = apis.sub_apis(index);
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        /* A GPU that cannot run an API cannot import for it either, whatever
         * the loader would answer. */
        kinds_[slot] = ((sub_apis_ & sub_api_bits[slot]) != 0u)
                           ? apis.import_kinds(index, sub_api_bits[slot])
                           : 0u;
    }
    index_ = index;
    known_ = true;
    return Status::ok;
}

bool HostGpu::matches(const uint8_t *uuid) const
{
    if (!known_ || uuid == nullptr) return false;
    for (uint32_t i = 0u; i < uuid_bytes; i++) {
        if (uuid_[i] != uuid[i]) return false;
    }
    return true;
}

uint32_t HostGpu::sub_apis() const { return sub_apis_; }

uint32_t HostGpu::import_kinds(uint32_t sub_api) const
{
    const uint32_t slot = slot_of(sub_api);
    return slot < sub_api_count ? kinds_[slot] : 0u;
}

uint32_t HostGpu::index() const { return index_; }

uint32_t HostGpu::slot_of(uint32_t sub_api) const
{
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        if (sub_api_bits[slot] == sub_api) return slot;
    }
    return sub_api_count;
}

bool HostGpu::invariants_hold() const
{
    if (!known_) return sub_apis_ == 0u;
    for (uint32_t slot = 0u; slot < sub_api_count; slot++) {
        if (kinds_[slot] != 0u && (sub_apis_ & sub_api_bits[slot]) == 0u) return false;
    }
    return true;
}

}  // namespace av
