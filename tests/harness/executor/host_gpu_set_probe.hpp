/* Reaches into a HostGpuSet so invariant tests can corrupt it, or one of the
 * GPUs it holds. Declared a friend by HostGpuSet; nothing outside the tests
 * uses it. */
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

}  // namespace av

#endif  // AV_HOST_GPU_SET_PROBE_HPP
