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
    template <typename Result>
    Result noted(const char *call, Result result) const;
    template <typename Result, typename... Parameters, typename... Arguments>
    Result traced(const char *slot, Result (Device::*method)(Parameters...), const Arguments &...arguments) const;
    template <typename... Parameters, typename... Arguments>
    void   announced(const char *slot, void (Device::*method)(Parameters...), const Arguments &...arguments) const;

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

template <typename Device>
template <typename Result>
Result DeviceCall<Device>::noted(const char *call, Result result) const
{
    AV_SCOPE("DeviceCall::noted");
    if (result < 0) device_.note_failure(call, static_cast<uint32_t>(result));
    return result;
}

template <typename Device>
template <typename Result, typename... Parameters, typename... Arguments>
Result DeviceCall<Device>::traced(const char *slot, Result (Device::*method)(Parameters...),
                                  const Arguments &...arguments) const
{
    AV_SCOPE("DeviceCall::traced");
    if constexpr (__is_same(Result, void)) {
        (device_.*method)(arguments...);
        device_.trace_call(slot, arguments...);
    } else {
        const Result result = (device_.*method)(arguments...);
        device_.trace_call(slot, arguments...);
        return result;
    }
}

template <typename Device>
template <typename... Parameters, typename... Arguments>
void DeviceCall<Device>::announced(const char *slot, void (Device::*method)(Parameters...),
                                   const Arguments &...arguments) const
{
    AV_SCOPE("DeviceCall::announced");
    device_.trace_call(slot, arguments...);
    (device_.*method)(arguments...);
}

}

#endif
