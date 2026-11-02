#ifndef AV_HOST_GPU_SET_PROBE_HPP
#define AV_HOST_GPU_SET_PROBE_HPP

#include "executor/host_gpu_set.hpp"

#include <stdint.h>

namespace av {

class HostGpu;

struct HostGpuSetProbe {
    explicit HostGpuSetProbe(HostGpuSet &target);

    HostGpu &gpu_at(uint32_t index);
    void     set_count(uint32_t count);

    HostGpuSet &set;
};

}

#endif
