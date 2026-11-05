#ifndef AV_D3D_HEADERS_HPP
#define AV_D3D_HEADERS_HPP

#include <d3d11_4.h>
#include <d3d9.h>
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vulkan/vulkan_core.h>

MIDL_INTERFACE("3a6d8f2c-b0e8-4ab4-b4dc-4fd24891bfa5")
IDXGIVkInteropAdapter : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance *instance, VkPhysicalDevice *physical_device) = 0;
};
__CRT_UUID_DECL(IDXGIVkInteropAdapter, 0x3a6d8f2c, 0xb0e8, 0x4ab4, 0xb4, 0xdc, 0x4f, 0xd2, 0x48, 0x91, 0xbf, 0xa5);

MIDL_INTERFACE("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")
ID3D9VkInteropInterface : public IUnknown {
    virtual void STDMETHODCALLTYPE GetInstanceHandle(VkInstance *instance) = 0;
    virtual void STDMETHODCALLTYPE GetPhysicalDeviceHandle(UINT adapter, VkPhysicalDevice *physical_device) = 0;
};
__CRT_UUID_DECL(ID3D9VkInteropInterface, 0x3461a81b, 0xce41, 0x485b, 0xb6, 0xb5, 0xfc, 0xf0, 0x8b, 0xa6, 0xa6, 0xbd);

MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(VkImage *handle, VkImageLayout *layout,
                                                         VkImageCreateInfo *info) = 0;
};
__CRT_UUID_DECL(ID3D9VkInteropTexture, 0xd56344f5, 0x8d35, 0x46fd, 0x80, 0x6d, 0x94, 0xc3, 0x51, 0xb4, 0x72, 0xc1);

MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance *instance, VkPhysicalDevice *physical_device,
                                                    VkDevice *device) = 0;
    virtual void STDMETHODCALLTYPE GetSubmissionQueue(VkQueue *queue, uint32_t *queue_index,
                                                      uint32_t *queue_family_index) = 0;
    virtual void STDMETHODCALLTYPE TransitionTextureLayout(ID3D9VkInteropTexture *texture,
                                                           const VkImageSubresourceRange *subresources,
                                                           VkImageLayout old_layout, VkImageLayout new_layout) = 0;
    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE LockDevice() = 0;
    virtual void STDMETHODCALLTYPE UnlockDevice() = 0;
    virtual bool STDMETHODCALLTYPE WaitForResource(IDirect3DResource9 *resource, DWORD map_flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateImage(const void *desc, IDirect3DResource9 **result) = 0;
};
__CRT_UUID_DECL(ID3D9VkInteropDevice, 0x2eaa4b89, 0x0107, 0x4bdb, 0x87, 0xf7, 0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0);

MIDL_INTERFACE("9c3a6f27-5b81-4e0d-a7d2-1f48c6b0e395")
ID3D9VkInteropDevice1 : public ID3D9VkInteropDevice {
    virtual HRESULT STDMETHODCALLTYPE BindBufferStorage(IDirect3DResource9 *resource, VkBuffer storage,
                                                        VkDeviceSize offset, VkDeviceSize length, void *map_ptr) = 0;
    virtual void STDMETHODCALLTYPE FlushCommands() = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferStorageBindable(IDirect3DResource9 *resource) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnbindBufferStorage(IDirect3DResource9 *resource) = 0;
    virtual HRESULT STDMETHODCALLTYPE MarkSubmission(void (*callback)(void *, uint64_t), void *context,
                                                     uint64_t value) = 0;
    virtual void STDMETHODCALLTYPE WaitForIdle() = 0;
};
__CRT_UUID_DECL(ID3D9VkInteropDevice1, 0x9c3a6f27, 0x5b81, 0x4e0d, 0xa7, 0xd2, 0x1f, 0x48, 0xc6, 0xb0, 0xe3, 0x95);

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

MIDL_INTERFACE("0ec870a6-5d7e-4c22-8cfc-5baae07616ed")
ID3D12CommandQueueTileMappings : public ID3D12Pageable {
    virtual void STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *resource, UINT region_count,
                                                      const D3D12_TILED_RESOURCE_COORDINATE *region_starts,
                                                      const D3D12_TILE_REGION_SIZE *region_sizes, ID3D12Heap *heap,
                                                      UINT range_count, const D3D12_TILE_RANGE_FLAGS *range_flags,
                                                      const UINT *heap_range_offsets, const UINT *range_tile_counts,
                                                      D3D12_TILE_MAPPING_FLAGS flags) = 0;
};

MIDL_INTERFACE("11ea7a1a-0f6a-49bf-b612-3e30f8e201dd")
ID3D12DeviceExt : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(VkInstance *instance, VkPhysicalDevice *physical_device,
                                                       VkDevice *device) = 0;
};
__CRT_UUID_DECL(ID3D12DeviceExt, 0x11ea7a1a, 0x0f6a, 0x49bf, 0xb6, 0x12, 0x3e, 0x30, 0xf8, 0xe2, 0x01, 0xdd);

constexpr GUID clsid_d3d12_device_factory = {0x114863bf, 0xc386, 0x4aee, {0xb3, 0x9d, 0x8f, 0x0b, 0xbb, 0x06, 0x29, 0x55}};

#endif
