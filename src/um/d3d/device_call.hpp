/* One runtime call into a device, holding the device's call lock for as long
 * as it lives. A free-threaded driver is called from any thread at once -
 * resource creation beside the immediate context's draws - so each device
 * object admits one call at a time; a deferred context is an object of its
 * own and records beside the others. A thunk makes one as a temporary, which
 * lives to the end of its full expression: the whole call. */
#ifndef AV_UM_DEVICE_CALL_HPP
#define AV_UM_DEVICE_CALL_HPP

namespace av::d3d11 {

class D3d11Device;

class DeviceCall {
public:
    explicit DeviceCall(void *block);
    ~DeviceCall();
    DeviceCall(const DeviceCall &) = delete;
    DeviceCall &operator=(const DeviceCall &) = delete;

    D3d11Device &device() const;

private:
    D3d11Device &device_;
};

}  // namespace av::d3d11

#endif  // AV_UM_DEVICE_CALL_HPP
