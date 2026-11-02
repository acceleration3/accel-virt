#include "harness/executor/widest_host_gpu.hpp"

#include "executor/shim/loader_host_apis.hpp"

namespace av {

WidestHostGpu::WidestHostGpu() : uuid_{}, sub_apis_(0u)
{
    LoaderHostApis host;
    host.open();
    for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
        const std::uint32_t runs = host.sub_apis(index);
        if (__builtin_popcount(runs) <= __builtin_popcount(sub_apis_)) continue;
        sub_apis_ = runs;
        host.gpu_uuid(index, uuid_);
    }
    host.close();
}

const std::uint8_t *WidestHostGpu::uuid() const { return uuid_; }
std::uint32_t       WidestHostGpu::sub_apis() const { return sub_apis_; }

}
