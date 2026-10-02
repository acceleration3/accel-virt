/* DXVK's Vulkan interop interfaces, as src/dxgi/dxgi_interfaces.h declares
 * them: that header pulls in DXVK's own sources, so the two this project uses
 * are declared again here, method for method, which is all a COM vtable is. */
#ifndef AV_DXVK_INTEROP_HPP
#define AV_DXVK_INTEROP_HPP

#include "executor/shim/d3d_headers.hpp"

#include <vulkan/vulkan.h>

struct IDXGIVkInteropSurface;

MIDL_INTERFACE("e2ef5fa5-dc21-4af7-90c4-f67ef6a09323")
IDXGIVkInteropDevice : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance *pInstance, VkPhysicalDevice *pPhysDev,
                                                    VkDevice *pDevice) = 0;
    virtual void STDMETHODCALLTYPE GetSubmissionQueue(VkQueue *pQueue, uint32_t *pQueueFamilyIndex) = 0;
    virtual void STDMETHODCALLTYPE TransitionSurfaceLayout(IDXGIVkInteropSurface *pSurface,
                                                           const VkImageSubresourceRange *pSubresources,
                                                           VkImageLayout OldLayout, VkImageLayout NewLayout) = 0;
    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
};

MIDL_INTERFACE("e2ef5fa5-dc21-4af7-90c4-f67ef6a09324")
IDXGIVkInteropDevice1 : public IDXGIVkInteropDevice {
    virtual void STDMETHODCALLTYPE GetSubmissionQueue1(VkQueue *pQueue, uint32_t *pQueueIndex,
                                                       uint32_t *pQueueFamilyIndex) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateTexture2DFromVkImage(const D3D11_TEXTURE2D_DESC1 *pDesc, VkImage vkImage,
                                                                 ID3D11Texture2D **ppTexture2D) = 0;
};

__CRT_UUID_DECL(IDXGIVkInteropDevice, 0xe2ef5fa5, 0xdc21, 0x4af7, 0x90, 0xc4, 0xf6, 0x7e, 0xf6, 0xa0, 0x93, 0x23);
__CRT_UUID_DECL(IDXGIVkInteropDevice1, 0xe2ef5fa5, 0xdc21, 0x4af7, 0x90, 0xc4, 0xf6, 0x7e, 0xf6, 0xa0, 0x93, 0x24);

#endif  // AV_DXVK_INTEROP_HPP
