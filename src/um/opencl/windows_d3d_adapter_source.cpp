#include "um/opencl/windows_d3d_adapter_source.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WindowsD3dAdapterSource::WindowsD3dAdapterSource()
    : adapter_iid_(__uuidof(IDXGIAdapter)), device_iid_(__uuidof(IDXGIDevice))
{
    AV_SCOPE("WindowsD3dAdapterSource::WindowsD3dAdapterSource");
}

uint64_t WindowsD3dAdapterSource::adapter_luid(void *dxgi_adapter)
{
    AV_SCOPE("WindowsD3dAdapterSource::adapter_luid");
    if (dxgi_adapter == nullptr) return 0u;
    IDXGIAdapter *adapter = nullptr;
    if (FAILED(static_cast<IUnknown *>(dxgi_adapter)->QueryInterface(adapter_iid_, reinterpret_cast<void **>(&adapter))))
        return 0u;
    const uint64_t luid = luid_of(*adapter);
    adapter->Release();
    return luid;
}

uint64_t WindowsD3dAdapterSource::device_luid(void *d3d10_device)
{
    AV_SCOPE("WindowsD3dAdapterSource::device_luid");
    if (d3d10_device == nullptr) return 0u;
    IDXGIDevice *device = nullptr;
    if (FAILED(static_cast<IUnknown *>(d3d10_device)->QueryInterface(device_iid_, reinterpret_cast<void **>(&device))))
        return 0u;
    IDXGIAdapter  *adapter = nullptr;
    const HRESULT  asked = device->GetAdapter(&adapter);
    device->Release();
    if (FAILED(asked) || adapter == nullptr) return 0u;
    const uint64_t luid = luid_of(*adapter);
    adapter->Release();
    return luid;
}

uint64_t WindowsD3dAdapterSource::luid_of(IDXGIAdapter &adapter)
{
    AV_SCOPE("WindowsD3dAdapterSource::luid_of");
    DXGI_ADAPTER_DESC description = {};
    if (FAILED(adapter.GetDesc(&description))) return 0u;
    return static_cast<uint64_t>(static_cast<uint32_t>(description.AdapterLuid.HighPart)) << 32u |
           description.AdapterLuid.LowPart;
}

}
