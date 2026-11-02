#ifndef AV_HOST_GPU_SET_HPP
#define AV_HOST_GPU_SET_HPP

#include "executor/host_gpu.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class HostApis;

class HostGpuSet {
public:
    static constexpr uint32_t max_gpus = 8u;

    HostGpuSet();
    HostGpuSet(const HostGpuSet &) = delete;
    HostGpuSet &operator=(const HostGpuSet &) = delete;

    void initialize(HostApis &apis);
    bool overflowed() const;

    uint32_t       count() const;
    void           report(StateWriter &writer) const;
    const HostGpu *find(const uint8_t *uuid) const;
    const HostGpu *at(uint32_t index) const;
    uint32_t       sub_apis() const;

    bool invariants_hold() const;

private:
    friend struct HostGpuSetProbe;

    HostGpu  gpus_[max_gpus];
    uint32_t count_;
    bool     overflowed_;
};

}

#endif
