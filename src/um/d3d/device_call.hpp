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
    /* `result`, a failure first noted by the device under the slot's name.
     * The device is locked before the call is made: the temporary is made
     * before the arguments the call is one of. */
    template <typename Result>
    Result noted(const char *call, Result result) const;
    /* `method` called with `arguments`, then the call to `slot` traced
     * (D3d11Device::trace_call): after it, so what it made has its id. */
    template <typename Result, typename... Parameters, typename... Arguments>
    Result traced(const char *slot, Result (Device::*method)(Parameters...), const Arguments &...arguments) const;
    /* The same traced before the call: a destroy forgets the id it names. */
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

}  // namespace av::d3d

#endif  // AV_UM_DEVICE_CALL_HPP
