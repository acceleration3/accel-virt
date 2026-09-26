#include "harness/executor/mock_host_apis.hpp"

#include "device/abi/rvk_executor.h"

namespace av {

MockHostApis::MockHostApis() : gpus_{}, count_(0u), questions_(0u), d3d11_(), d3d11_index_(max_gpus) {}

void MockHostApis::add_gpu(const uint8_t *uuid, uint32_t sub_apis, uint32_t vulkan_kinds,
                           uint32_t opengl_kinds, uint32_t opencl_kinds)
{
    if (count_ >= max_gpus) return;
    Gpu &gpu = gpus_[count_];
    for (uint32_t i = 0u; i < uuid_bytes; i++) gpu.uuid[i] = uuid[i];
    gpu.sub_apis = sub_apis;
    gpu.kinds[0] = vulkan_kinds;
    gpu.kinds[1] = opengl_kinds;
    gpu.kinds[2] = opencl_kinds;
    count_++;
}

uint32_t MockHostApis::questions() const { return questions_; }

void MockHostApis::give_d3d11(uint32_t index) { d3d11_index_ = index; }

D3d11HostDevice &MockHostApis::d3d11() { return d3d11_; }

uint32_t MockHostApis::gpu_count()
{
    questions_++;
    return count_;
}

void MockHostApis::gpu_uuid(uint32_t index, uint8_t *out)
{
    questions_++;
    if (index >= count_ || out == nullptr) return;
    for (uint32_t i = 0u; i < uuid_bytes; i++) out[i] = gpus_[index].uuid[i];
}

int32_t MockHostApis::render_minor(uint32_t)
{
    questions_++;
    return -1;
}

uint32_t MockHostApis::sub_apis(uint32_t index)
{
    questions_++;
    return index < count_ ? gpus_[index].sub_apis : 0u;
}

uint32_t MockHostApis::import_kinds(uint32_t index, uint32_t sub_api)
{
    questions_++;
    if (index >= count_) return 0u;
    if (sub_api == RVK_EXECUTOR_SUB_API_VULKAN) return gpus_[index].kinds[0];
    if (sub_api == RVK_EXECUTOR_SUB_API_OPENGL) return gpus_[index].kinds[1];
    if (sub_api == RVK_EXECUTOR_SUB_API_OPENCL) return gpus_[index].kinds[2];
    return 0u;
}

const GlDisplay *MockHostApis::gl_display(uint32_t)
{
    questions_++;
    return nullptr;
}

const OpenClDevice *MockHostApis::opencl_device(uint32_t)
{
    questions_++;
    return nullptr;
}

D3d11HostDevice *MockHostApis::d3d11_device(uint32_t index)
{
    questions_++;
    return index == d3d11_index_ ? &d3d11_ : nullptr;
}

D3d12HostDevice *MockHostApis::d3d12_device(uint32_t)
{
    questions_++;
    return nullptr;
}

}  // namespace av
