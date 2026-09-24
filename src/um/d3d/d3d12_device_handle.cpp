#include "um/d3d/d3d12_device_handle.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/device_table.hpp"

namespace av::d3d12 {

D3d12DeviceHandle::D3d12DeviceHandle(void *block) : block_(block)
{
    AV_SCOPE("D3d12DeviceHandle::D3d12DeviceHandle");
}

D3d12Device &D3d12DeviceHandle::device() const
{
    AV_SCOPE("D3d12DeviceHandle::device");
    D3d12Device *device = nullptr;
    __builtin_memcpy(&device, static_cast<const char *>(block_) + d3d::DeviceTable::extension_offset, sizeof(device));
    return *device;
}

void D3d12DeviceHandle::bind(D3d12Device &device)
{
    AV_SCOPE("D3d12DeviceHandle::bind");
    D3d12Device *const address = &device;
    __builtin_memcpy(static_cast<char *>(block_) + d3d::DeviceTable::extension_offset, &address, sizeof(address));
}

}  // namespace av::d3d12
