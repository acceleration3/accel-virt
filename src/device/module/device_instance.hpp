/* A device and the host operations it was created over: the QEMU glue's table
 * outlives the device, and the TableHost over it lives exactly as long. */
#ifndef AV_DEVICE_INSTANCE_HPP
#define AV_DEVICE_INSTANCE_HPP

#include "device/abi/table_host.hpp"
#include "device/module/virtio_gpu_rvk.hpp"

namespace av {

class DeviceInstance {
public:
    explicit DeviceInstance(const RvkHostOps &ops);
    DeviceInstance(const DeviceInstance &) = delete;
    DeviceInstance &operator=(const DeviceInstance &) = delete;

    VirtioGpuRvk &device();

private:
    TableHost    host_;
    VirtioGpuRvk device_;
};

}  // namespace av

#endif  // AV_DEVICE_INSTANCE_HPP
