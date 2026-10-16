/* The device a runtime's device block names: the address the driver wrote
 * into the block's extension slot (DeviceTable::extension_offset) when it made
 * the device. Every table thunk of Direct3D 11 and 12 goes through one of
 * these, so it is a read of one word - no search, no lock. The block is the
 * runtime's and was the driver's to write; a thunk is only ever reached with a
 * block the runtime got from a successful CreateDevice. */
#ifndef AV_D3D_DEVICE_BLOCK_HPP
#define AV_D3D_DEVICE_BLOCK_HPP

#include "core/trace/function_scope.hpp"
#include "icd/d3d/device_table.hpp"

namespace av::d3d {

template <typename Device>
class DeviceBlock {
public:
    explicit DeviceBlock(void *block);

    Device &device() const;
    /* Writes where `device` lives into the block. */
    void    bind(Device &device);

private:
    void *block_;
};

template <typename Device>
DeviceBlock<Device>::DeviceBlock(void *block) : block_(block)
{
    AV_SCOPE("DeviceBlock::DeviceBlock");
}

template <typename Device>
Device &DeviceBlock<Device>::device() const
{
    AV_SCOPE("DeviceBlock::device");
    Device *device = nullptr;
    __builtin_memcpy(&device, static_cast<const char *>(block_) + DeviceTable::extension_offset, sizeof(device));
    return *device;
}

template <typename Device>
void DeviceBlock<Device>::bind(Device &device)
{
    AV_SCOPE("DeviceBlock::bind");
    Device *const address = &device;
    __builtin_memcpy(static_cast<char *>(block_) + DeviceTable::extension_offset, &address, sizeof(address));
}

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_BLOCK_HPP
