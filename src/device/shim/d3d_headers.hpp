/* The Direct3D 11 and DXGI API as DXVK's native build declares it, and the one
 * DXVK interop interface the executor calls - declared here rather than taken
 * from DXVK's source tree, whose header drags in the whole of DXVK's internals.
 *
 * Included only by translation units that call into DXVK. Its headers define
 * the Windows base types and a handful of macros, so nothing else in the
 * device may see them. */
#ifndef AV_D3D_HEADERS_HPP
#define AV_D3D_HEADERS_HPP

#include <d3d11_4.h>
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

#endif  // AV_D3D_HEADERS_HPP
