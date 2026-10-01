#include "um/d3d/d3d9_device_call.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d9_device.hpp"

namespace av::d3d9 {

D3d9DeviceCall::D3d9DeviceCall(void *device) : device_(*static_cast<D3d9Device *>(device))
{
    AV_SCOPE("D3d9DeviceCall::D3d9DeviceCall");
    device_.lock_calls();
}

D3d9DeviceCall::~D3d9DeviceCall()
{
    AV_SCOPE("D3d9DeviceCall::~D3d9DeviceCall");
    device_.unlock_calls();
}

D3d9Device &D3d9DeviceCall::device() const
{
    AV_SCOPE("D3d9DeviceCall::device");
    return device_;
}

}  // namespace av::d3d9
