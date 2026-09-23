/* The device a runtime's device block names: the address the driver wrote
 * into the block's extension slot (DeviceTable::extension_offset) when it made
 * the device. Every table thunk goes through one of these, so it is a read of
 * one word - no search, no lock. The block is the runtime's and was the
 * driver's to write; a thunk is only ever reached with a block the runtime got
 * from a successful CreateDevice. */
#ifndef AV_UM_DEVICE_HANDLE_HPP
#define AV_UM_DEVICE_HANDLE_HPP

namespace av::d3d11 {

class D3d11Device;

class DeviceHandle {
public:
    explicit DeviceHandle(void *block);

    D3d11Device &device() const;
    /* Writes where `device` lives into the block. */
    void         bind(D3d11Device &device);

private:
    void *block_;
};

}  // namespace av::d3d11

#endif  // AV_UM_DEVICE_HANDLE_HPP
