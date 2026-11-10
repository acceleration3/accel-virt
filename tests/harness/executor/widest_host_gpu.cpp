#include "harness/executor/widest_host_gpu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "executor/executor_log.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/icd/recording_log.hpp"

namespace av {

WidestHostGpu::WidestHostGpu() : uuid_{}, sub_apis_(0u)
{
    RecordingLog   sink;
    ExecutorLog    log(sink);
    LoaderHostApis host(log);
    host.open();
    const char *const wanted = std::getenv("AV_TEST_HOST_GPU");
    for (std::uint32_t index = 0u; index < host.gpu_count(); index++) {
        const std::uint32_t runs = host.sub_apis(index);
        if (wanted != nullptr) {
            std::uint8_t candidate[16] = {};
            host.gpu_uuid(index, candidate);
            char text[33] = {};
            for (std::uint32_t at = 0u; at < 16u; at++) std::snprintf(text + 2u * at, 3u, "%02x", candidate[at]);
            if (std::strcmp(text, wanted) != 0) continue;
        } else if (__builtin_popcount(runs) <= __builtin_popcount(sub_apis_)) continue;
        sub_apis_ = runs;
        host.gpu_uuid(index, uuid_);
    }
    host.close();
}

const std::uint8_t *WidestHostGpu::uuid() const { return uuid_; }
std::uint32_t       WidestHostGpu::sub_apis() const { return sub_apis_; }

}
