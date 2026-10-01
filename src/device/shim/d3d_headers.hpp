/* The Direct3D 9, 11 and DXGI API as DXVK's native build declares it, and the
 * DXVK interop interfaces the executor calls - declared here rather than taken
 * from DXVK's source tree, whose header drags in the whole of DXVK's internals.
 *
 * Included only by translation units that call into DXVK. Its headers define
 * the Windows base types and a handful of macros, so nothing else in the
 * device may see them. */
#ifndef AV_D3D_HEADERS_HPP
#define AV_D3D_HEADERS_HPP

#include <d3d11_4.h>
#include <d3d9.h>
/* vkd3d-proton returns a struct from a COM method through a pointer, as the
 * Windows ABI does (vkd3d_d3d12.idl defines this for itself); the native
 * d3d12.h declares that shape only when asked. Only d3d12.h has such methods. */
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vulkan/vulkan_core.h>

/* DXVK's IDXGIVkInteropAdapter (src/dxgi/dxgi_interfaces.h): which Vulkan
 * physical device a DXGI adapter is, which is how the executor puts a device
 * on the GPU the guest's adapter stands for. */
MIDL_INTERFACE("3a6d8f2c-b0e8-4ab4-b4dc-4fd24891bfa5")
IDXGIVkInteropAdapter : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance *instance, VkPhysicalDevice *physical_device) = 0;
};
__CRT_UUID_DECL(IDXGIVkInteropAdapter, 0x3a6d8f2c, 0xb0e8, 0x4ab4, 0xb4, 0xdc, 0x4f, 0xd2, 0x48, 0x91, 0xbf, 0xa5);

/* DXVK's ID3D9VkInteropInterface (src/d3d9/d3d9_interfaces.h): which Vulkan
 * physical device a Direct3D 9 adapter ordinal is. */
MIDL_INTERFACE("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")
ID3D9VkInteropInterface : public IUnknown {
    virtual void STDMETHODCALLTYPE GetInstanceHandle(VkInstance *instance) = 0;
    virtual void STDMETHODCALLTYPE GetPhysicalDeviceHandle(UINT adapter, VkPhysicalDevice *physical_device) = 0;
};
__CRT_UUID_DECL(ID3D9VkInteropInterface, 0x3461a81b, 0xce41, 0x485b, 0xb6, 0xb5, 0xfc, 0xf0, 0x8b, 0xa6, 0xa6, 0xbd);

/* ID3D12DeviceFactory (d3d12.h of the Agility SDK, vkd3d-proton's
 * include/vkd3d_d3d12.idl), which DXVK's native d3d12.h predates: how the
 * executor makes one independent device per GPU. */
MIDL_INTERFACE("61f307d3-d34e-4e7c-8374-3ba4de23cccb")
ID3D12DeviceFactory : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE InitializeFromGlobalState() = 0;
    virtual HRESULT STDMETHODCALLTYPE ApplyToGlobalState() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFlags(UINT flags) = 0;
    virtual UINT STDMETHODCALLTYPE    GetFlags() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConfigurationInterface(REFCLSID clsid, REFIID iid, void **object) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnableExperimentalFeatures(UINT count, const IID *iids, void *configurations,
                                                                 UINT *sizes) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateDevice(IUnknown *adapter, D3D_FEATURE_LEVEL level, REFIID riid,
                                                   void **device) = 0;
};
__CRT_UUID_DECL(ID3D12DeviceFactory, 0x61f307d3, 0xd34e, 0x4e7c, 0x83, 0x74, 0x3b, 0xa4, 0xde, 0x23, 0xcc, 0xcb);

/* CLSID_D3D12DeviceFactory, what D3D12GetInterface makes a factory from. */
constexpr GUID clsid_d3d12_device_factory = {0x114863bf, 0xc386, 0x4aee, {0xb3, 0x9d, 0x8f, 0x0b, 0xbb, 0x06, 0x29, 0x55}};

#endif  // AV_D3D_HEADERS_HPP
