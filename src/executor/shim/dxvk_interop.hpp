/* DXVK's Vulkan interop interfaces and its context extensions, as
 * src/dxgi/dxgi_interfaces.h and src/d3d11/d3d11_interfaces.h declare them:
 * those headers pull in DXVK's own sources, so the ones this project uses are
 * declared again here, method for method, which is all a COM vtable is. */
#ifndef AV_DXVK_INTEROP_HPP
#define AV_DXVK_INTEROP_HPP

#include "executor/shim/d3d_headers.hpp"

#include <vulkan/vulkan.h>

struct IDXGIVkInteropDevice;

MIDL_INTERFACE("5546cf8c-77e7-4341-b05d-8d4d5000e77d")
IDXGIVkInteropSurface : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDXGIVkInteropDevice **ppDevice) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(VkImage *pHandle, VkImageLayout *pLayout,
                                                         VkImageCreateInfo *pInfo) = 0;
};

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

/* DXVK's own extensions of a Direct3D 11 context, declared in its vtable's
 * order: only BindBufferStorage is called here (tools/dxvk-patches/0003). */
MIDL_INTERFACE("fd0bca13-5cb6-4c3a-987e-4750de2ca791")
ID3D11VkExtContext : public IUnknown {
    virtual void STDMETHODCALLTYPE MultiDrawIndirect(UINT DrawCount, ID3D11Buffer *pBufferForArgs, UINT ByteOffsetForArgs,
                                                     UINT ByteStrideForArgs) = 0;
    virtual void STDMETHODCALLTYPE MultiDrawIndexedIndirect(UINT DrawCount, ID3D11Buffer *pBufferForArgs,
                                                            UINT ByteOffsetForArgs, UINT ByteStrideForArgs) = 0;
    virtual void STDMETHODCALLTYPE MultiDrawIndirectCount(UINT MaxDrawCount, ID3D11Buffer *pBufferForCount,
                                                          UINT ByteOffsetForCount, ID3D11Buffer *pBufferForArgs,
                                                          UINT ByteOffsetForArgs, UINT ByteStrideForArgs) = 0;
    virtual void STDMETHODCALLTYPE MultiDrawIndexedIndirectCount(UINT MaxDrawCount, ID3D11Buffer *pBufferForCount,
                                                                 UINT ByteOffsetForCount, ID3D11Buffer *pBufferForArgs,
                                                                 UINT ByteOffsetForArgs, UINT ByteStrideForArgs) = 0;
    virtual void STDMETHODCALLTYPE SetDepthBoundsTest(BOOL Enable, FLOAT MinDepthBounds, FLOAT MaxDepthBounds) = 0;
    virtual void STDMETHODCALLTYPE SetBarrierControl(UINT ControlFlags) = 0;
};

MIDL_INTERFACE("874b09b2-ae0b-41d8-8476-5f3b7a0e879d")
ID3D11VkExtContext1 : public ID3D11VkExtContext {
    virtual bool STDMETHODCALLTYPE LaunchCubinShaderNVX(IUnknown *hShader, uint32_t gridX, uint32_t gridY,
                                                        uint32_t gridZ, const void *pParams, uint32_t paramSize,
                                                        void *const *pReadResources, uint32_t numReadResources,
                                                        void *const *pWriteResources, uint32_t numWriteResources) = 0;
};

MIDL_INTERFACE("4f0b8a51-7d1e-4c8a-9b0e-6a2c3f5d8e17")
ID3D11VkExtContext2 : public ID3D11VkExtContext1 {
    virtual HRESULT STDMETHODCALLTYPE BindBufferStorage(ID3D11Buffer *pBuffer, VkBuffer Storage, VkDeviceSize Offset,
                                                        VkDeviceSize Length, void *pMapPtr) = 0;
};

__CRT_UUID_DECL(ID3D11VkExtContext, 0xfd0bca13, 0x5cb6, 0x4c3a, 0x98, 0x7e, 0x47, 0x50, 0xde, 0x2c, 0xa7, 0x91);
__CRT_UUID_DECL(ID3D11VkExtContext1, 0x874b09b2, 0xae0b, 0x41d8, 0x84, 0x76, 0x5f, 0x3b, 0x7a, 0x0e, 0x87, 0x9d);
__CRT_UUID_DECL(ID3D11VkExtContext2, 0x4f0b8a51, 0x7d1e, 0x4c8a, 0x9b, 0x0e, 0x6a, 0x2c, 0x3f, 0x5d, 0x8e, 0x17);
__CRT_UUID_DECL(IDXGIVkInteropSurface, 0x5546cf8c, 0x77e7, 0x4341, 0xb0, 0x5d, 0x8d, 0x4d, 0x50, 0x00, 0xe7, 0x7d);
__CRT_UUID_DECL(IDXGIVkInteropDevice, 0xe2ef5fa5, 0xdc21, 0x4af7, 0x90, 0xc4, 0xf6, 0x7e, 0xf6, 0xa0, 0x93, 0x23);
__CRT_UUID_DECL(IDXGIVkInteropDevice1, 0xe2ef5fa5, 0xdc21, 0x4af7, 0x90, 0xc4, 0xf6, 0x7e, 0xf6, 0xa0, 0x93, 0x24);

#endif  // AV_DXVK_INTEROP_HPP
