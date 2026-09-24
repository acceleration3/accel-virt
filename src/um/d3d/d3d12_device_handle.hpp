/* The Direct3D 12 device a runtime's device block names: the address the
 * driver wrote into the block's extension slot (DeviceTable::extension_offset)
 * when it made the device, read by every device-table thunk. The Direct3D 11
 * DeviceHandle's twin, over the other runtime's device. */
#ifndef AV_UM_D3D12_DEVICE_HANDLE_HPP
#define AV_UM_D3D12_DEVICE_HANDLE_HPP

namespace av::d3d12 {

class D3d12Device;

class D3d12DeviceHandle {
public:
    explicit D3d12DeviceHandle(void *block);
    D3d12Device &device() const;
    /* Writes where `device` lives into the block. */
    void         bind(D3d12Device &device);

private:
    void *block_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_DEVICE_HANDLE_HPP
