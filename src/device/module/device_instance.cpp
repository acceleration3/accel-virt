#include "device/module/device_instance.hpp"

namespace av {

DeviceInstance::DeviceInstance(const RvkHostOps &ops) : host_(ops), device_(host_) {}

VirtioGpuRvk &DeviceInstance::device()
{
    return device_;
}

}  // namespace av
