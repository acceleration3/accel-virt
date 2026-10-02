/* One runtime call into a device, holding the device's call lock for as long
 * as it lives (ReentrantCallLock). A thunk makes one as a temporary, which
 * lives to the end of its full expression: the whole call. Direct3D 9's and
 * 11's devices each take their calls through one; a Direct3D 11 deferred
 * context is a device object of its own and records beside the others. */
#ifndef AV_UM_DEVICE_CALL_HPP
#define AV_UM_DEVICE_CALL_HPP

#include "core/trace/function_scope.hpp"

namespace av::d3d {

template <typename Device>
class DeviceCall {
public:
    explicit DeviceCall(Device &device);
    ~DeviceCall();
    DeviceCall(const DeviceCall &) = delete;
    DeviceCall &operator=(const DeviceCall &) = delete;

    Device &device() const;

private:
    Device &device_;
};

template <typename Device>
DeviceCall<Device>::DeviceCall(Device &device) : device_(device)
{
    AV_SCOPE("DeviceCall::DeviceCall");
    device_.lock_calls();
}

template <typename Device>
DeviceCall<Device>::~DeviceCall()
{
    AV_SCOPE("DeviceCall::~DeviceCall");
    device_.unlock_calls();
}

template <typename Device>
Device &DeviceCall<Device>::device() const
{
    AV_SCOPE("DeviceCall::device");
    return device_;
}

}  // namespace av::d3d

#endif  // AV_UM_DEVICE_CALL_HPP
