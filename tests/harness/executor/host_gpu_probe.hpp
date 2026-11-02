#ifndef AV_HOST_GPU_PROBE_HPP
#define AV_HOST_GPU_PROBE_HPP

#include "executor/host_gpu.hpp"

#include <stdint.h>

namespace av {

struct HostGpuProbe {
    explicit HostGpuProbe(HostGpu &target);

    void set_sub_apis(uint32_t sub_apis);

    HostGpu &gpu;
};

}

#endif
