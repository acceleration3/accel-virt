/* One runtime call into a Direct3D 9 device, holding the device's call lock
 * for as long as it lives: d3d9.dll calls a multithreaded device from any
 * thread. A thunk makes one as a temporary, which lives to the end of its
 * full expression: the whole call. */
#ifndef AV_UM_D3D9_DEVICE_CALL_HPP
#define AV_UM_D3D9_DEVICE_CALL_HPP

namespace av::d3d9 {

class D3d9Device;

class D3d9DeviceCall {
public:
    /* `device` is the handle CreateDevice handed back: the device itself. */
    explicit D3d9DeviceCall(void *device);
    ~D3d9DeviceCall();
    D3d9DeviceCall(const D3d9DeviceCall &) = delete;
    D3d9DeviceCall &operator=(const D3d9DeviceCall &) = delete;

    D3d9Device &device() const;

private:
    D3d9Device &device_;
};

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_DEVICE_CALL_HPP
