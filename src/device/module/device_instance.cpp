#include "device/module/device_instance.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceInstance::DeviceInstance(const AvHostOps &ops) : host_(ops), device_(host_)
{
    AV_SCOPE("DeviceInstance::DeviceInstance");
}

VirtioGpuAv &DeviceInstance::device()
{
    AV_SCOPE("DeviceInstance::device");
    return device_;
}

}  // namespace av
