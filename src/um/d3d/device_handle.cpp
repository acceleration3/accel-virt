#include "um/d3d/device_handle.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/device_table.hpp"

namespace av::d3d11 {

DeviceHandle::DeviceHandle(void *block) : block_(block)
{
    AV_SCOPE("DeviceHandle::DeviceHandle");
}

D3d11Device &DeviceHandle::device() const
{
    AV_SCOPE("DeviceHandle::device");
    D3d11Device *device = nullptr;
    __builtin_memcpy(&device, static_cast<const char *>(block_) + d3d::DeviceTable::extension_offset, sizeof(device));
    return *device;
}

void DeviceHandle::bind(D3d11Device &device)
{
    AV_SCOPE("DeviceHandle::bind");
    D3d11Device *const address = &device;
    __builtin_memcpy(static_cast<char *>(block_) + d3d::DeviceTable::extension_offset, &address, sizeof(address));
}

}  // namespace av::d3d11
