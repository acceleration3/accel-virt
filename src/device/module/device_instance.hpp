#ifndef AV_DEVICE_INSTANCE_HPP
#define AV_DEVICE_INSTANCE_HPP

#include "device/host/table_host.hpp"
#include "device/module/virtio_gpu_av.hpp"

namespace av {

class DeviceInstance {
public:
    explicit DeviceInstance(const AvHostOps &ops);
    DeviceInstance(const DeviceInstance &) = delete;
    DeviceInstance &operator=(const DeviceInstance &) = delete;

    VirtioGpuAv &device();

private:
    TableHost    host_;
    VirtioGpuAv device_;
};

}

#endif
