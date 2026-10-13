/* vkd3d-proton's Vulkan interop interface, as include/vkd3d_device_vkd3d_ext.idl
 * declares it: the header its IDL makes pulls in vkd3d's own build, so the
 * interface is declared again here, method for method, which is all a COM
 * vtable is. */
#ifndef AV_VKD3D_INTEROP_HPP
#define AV_VKD3D_INTEROP_HPP

#include "executor/shim/d3d_headers.hpp"

#include <vulkan/vulkan.h>

MIDL_INTERFACE("39da4e09-bd1c-4198-9fae-86bbe3be41fd")
ID3D12DXVKInteropDevice : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(REFIID iid, void **object) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(UINT *extension_count, const char **extensions) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(UINT *extension_count, const char **extensions) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(const VkPhysicalDeviceFeatures2 **features) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(VkInstance *vk_instance, VkPhysicalDevice *vk_physical_device,
                                                       VkDevice *vk_device) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfo(ID3D12CommandQueue *queue, VkQueue *vk_queue,
                                                         UINT32 *vk_queue_family) = 0;
    virtual void STDMETHODCALLTYPE    GetVulkanImageLayout(ID3D12Resource *resource, D3D12_RESOURCE_STATES state,
                                                           VkImageLayout *vk_layout) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo(ID3D12Resource *resource, UINT64 *vk_handle,
                                                            UINT64 *buffer_offset) = 0;
    virtual HRESULT STDMETHODCALLTYPE LockCommandQueue(ID3D12CommandQueue *queue) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockCommandQueue(ID3D12CommandQueue *queue) = 0;
};

__CRT_UUID_DECL(ID3D12DXVKInteropDevice, 0x39da4e09, 0xbd1c, 0x4198, 0x9f, 0xae, 0x86, 0xbb, 0xe3, 0xbe, 0x41, 0xfd);

#endif  // AV_VKD3D_INTEROP_HPP
