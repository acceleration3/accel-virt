#include "um/d3d/device_call.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

DeviceCall::DeviceCall(void *block) : device_(DeviceHandle(block).device())
{
    AV_SCOPE("DeviceCall::DeviceCall");
    device_.lock_calls();
}

DeviceCall::~DeviceCall()
{
    AV_SCOPE("DeviceCall::~DeviceCall");
    device_.unlock_calls();
}

D3d11Device &DeviceCall::device() const
{
    AV_SCOPE("DeviceCall::device");
    return device_;
}

}  // namespace av::d3d11
