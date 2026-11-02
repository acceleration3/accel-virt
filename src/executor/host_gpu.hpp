#ifndef AV_HOST_GPU_HPP
#define AV_HOST_GPU_HPP

#include "proto/context/device_uuid.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class HostApis;

class HostGpu {
public:
    HostGpu();
    HostGpu(const HostGpu &) = delete;
    HostGpu &operator=(const HostGpu &) = delete;

    void describe(HostApis &apis, uint32_t index);

    bool     matches(const uint8_t *uuid) const;
    uint32_t sub_apis() const;
    uint32_t index() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct HostGpuProbe;

    DeviceUuid uuid_;
    uint32_t sub_apis_;
    uint32_t index_;
    bool     known_;
};

}

#endif
