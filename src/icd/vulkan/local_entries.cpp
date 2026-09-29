#include "icd/vulkan/local_entries.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

namespace av::icd {

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion)
{
    AV_SCOPE("local_vk_icdNegotiateLoaderICDInterfaceVersion");
    return icd_binding.root().negotiate(pSupportedVersion);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
    AV_SCOPE("local_vkCreateInstance");
    return icd_binding.root().create_instance(pCreateInfo, pAllocator, pInstance);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceExtensionProperties(const char *pLayerName,
                                                                            uint32_t *pPropertyCount,
                                                                            VkExtensionProperties *pProperties)
{
    AV_SCOPE("local_vkEnumerateInstanceExtensionProperties");
    return icd_binding.root().enumerate_instance_extensions(pLayerName, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceVersion(uint32_t *pApiVersion)
{
    AV_SCOPE("local_vkEnumerateInstanceVersion");
    return icd_binding.root().enumerate_instance_version(pApiVersion);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vk_icdGetPhysicalDeviceProcAddr(VkInstance, const char *pName)
{
    AV_SCOPE("local_vk_icdGetPhysicalDeviceProcAddr");
    return icd_binding.root().physical_device_proc_addr(pName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
    AV_SCOPE("local_vkGetDeviceProcAddr");
    return icd_binding.root().device_proc_addr(device, pName);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdEnumerateAdapterPhysicalDevices(VkInstance instance, uint64_t adapterLUID,
                                                                           uint32_t *pPhysicalDeviceCount,
                                                                           VkPhysicalDevice *pPhysicalDevices)
{
    AV_SCOPE("local_vk_icdEnumerateAdapterPhysicalDevices");
    return icd_binding.root().enumerate_adapter_physical_devices(instance, adapterLUID, pPhysicalDeviceCount,
                                                                pPhysicalDevices); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                          const char *pLayerName,
                                                                          uint32_t *pPropertyCount,
                                                                          VkExtensionProperties *pProperties)
{
    AV_SCOPE("local_vkEnumerateDeviceExtensionProperties");
    return icd_binding.root().enumerate_device_extensions(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
    AV_SCOPE("local_vkCreateDevice");
    return icd_binding.root().create_device(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkDestroyDevice");
    icd_binding.root().destroy_device(device, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    AV_SCOPE("local_vkCreateDescriptorUpdateTemplate");
    return icd_binding.root().create_descriptor_update_template(device, pCreateInfo, pAllocator,
                                                                pDescriptorUpdateTemplate);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDescriptorUpdateTemplateKHR(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    AV_SCOPE("local_vkCreateDescriptorUpdateTemplateKHR");
    return icd_binding.root().create_descriptor_update_template(device, pCreateInfo, pAllocator,
                                                                pDescriptorUpdateTemplate);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroyDescriptorUpdateTemplate(VkDevice device,
                                                                   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                                   const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkDestroyDescriptorUpdateTemplate");
    icd_binding.root().destroy_descriptor_update_template(device, descriptorUpdateTemplate, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroyDescriptorUpdateTemplateKHR(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkDestroyDescriptorUpdateTemplateKHR");
    icd_binding.root().destroy_descriptor_update_template(device, descriptorUpdateTemplate, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceProperties *pProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceProperties");
    icd_binding.root().physical_device_properties(physicalDevice, pProperties);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                                VkPhysicalDeviceProperties2 *pProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceProperties2");
    icd_binding.root().physical_device_properties2(physicalDevice, pProperties);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                                  VkQueue *pQueue)
{
    AV_SCOPE("local_vkGetDeviceQueue");
    icd_binding.root().device_queue(device, queueFamilyIndex, queueIndex, pQueue);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo,
                                                   VkQueue *pQueue)
{
    AV_SCOPE("local_vkGetDeviceQueue2");
    icd_binding.root().device_queue2(device, pQueueInfo, pQueue);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkAllocateCommandBuffers(VkDevice device,
                                                              const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                              VkCommandBuffer *pCommandBuffers)
{
    AV_SCOPE("local_vkAllocateCommandBuffers");
    return icd_binding.root().allocate_command_buffers(device, pAllocateInfo, pCommandBuffers);
}

VKAPI_ATTR void VKAPI_CALL local_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                      uint32_t commandBufferCount,
                                                      const VkCommandBuffer *pCommandBuffers)
{
    AV_SCOPE("local_vkFreeCommandBuffers");
    icd_binding.root().free_command_buffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                          const VkCommandBufferBeginInfo *pBeginInfo)
{
    AV_SCOPE("local_vkBeginCommandBuffer");
    return icd_binding.root().begin_command_buffer(commandBuffer, pBeginInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
    AV_SCOPE("local_vkEndCommandBuffer");
    return icd_binding.root().end_command_buffer(commandBuffer);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                                          VkCommandBufferResetFlags flags)
{
    AV_SCOPE("local_vkResetCommandBuffer");
    return icd_binding.root().reset_command_buffer(commandBuffer, flags);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences,
                                                     VkBool32 waitAll, uint64_t timeout)
{
    AV_SCOPE("local_vkWaitForFences");
    return icd_binding.root().wait_for_fences(device, fenceCount, pFences, waitAll, timeout);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo,
                                                      uint64_t timeout)
{
    AV_SCOPE("local_vkWaitSemaphores");
    return icd_binding.root().wait_semaphores(device, pWaitInfo, timeout);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkQueueWaitIdle(VkQueue queue)
{
    AV_SCOPE("local_vkQueueWaitIdle");
    return icd_binding.root().queue_wait_idle(queue);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkDeviceWaitIdle(VkDevice device)
{
    AV_SCOPE("local_vkDeviceWaitIdle");
    return icd_binding.root().device_wait_idle(device);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkRvkForceContextFault(VkPhysicalDevice physicalDevice)
{
    AV_SCOPE("local_vkRvkForceContextFault");
    return icd_binding.root().force_context_fault(physicalDevice);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkRvkLastKernelStatus(VkPhysicalDevice physicalDevice, uint32_t *status)
{
    AV_SCOPE("local_vkRvkLastKernelStatus");
    return icd_binding.root().last_kernel_status(physicalDevice, status);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkDestroyInstance");
    icd_binding.root().destroy_instance(instance, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                                     VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceMemoryProperties");
    icd_binding.root().physical_device_memory_properties(physicalDevice, pMemoryProperties);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                                      VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceMemoryProperties2");
    icd_binding.root().physical_device_memory_properties2(physicalDevice, pMemoryProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory)
{
    AV_SCOPE("local_vkAllocateMemory");
    return icd_binding.root().allocate_memory(device, pAllocateInfo, pAllocator, pMemory);
}

VKAPI_ATTR void VKAPI_CALL local_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                              const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkFreeMemory");
    icd_binding.root().free_memory(device, memory, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                                 VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    AV_SCOPE("local_vkMapMemory");
    return icd_binding.root().map_memory(device, memory, offset, size, flags, ppData);
}

VKAPI_ATTR void VKAPI_CALL local_vkUnmapMemory(VkDevice, VkDeviceMemory)
{
    AV_SCOPE("local_vkUnmapMemory");
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfoKHR *pMemoryMapInfo,
                                                     void **ppData)
{
    AV_SCOPE("local_vkMapMemory2KHR");
    return icd_binding.root().map_memory2(device, pMemoryMapInfo, ppData);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkUnmapMemory2KHR(VkDevice, const VkMemoryUnmapInfoKHR *pMemoryUnmapInfo)
{
    AV_SCOPE("local_vkUnmapMemory2KHR");
    /* Unmapping frees nothing: the chunk's view lives as long as the chunk. A
     * flag in the request is a caller asking for something else - reserving
     * the range - which this driver does not do. */
    if (pMemoryUnmapInfo == nullptr || pMemoryUnmapInfo->flags != 0u) return VK_ERROR_MEMORY_MAP_FAILED;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetMemoryWin32HandleKHR(VkDevice device,
                                                               const MemoryGetWin32HandleRecord *pGetWin32HandleInfo,
                                                               void                            **pHandle)
{
    AV_SCOPE("local_vkGetMemoryWin32HandleKHR");
    return icd_binding.root().export_memory_handle(device, pGetWin32HandleInfo, pHandle);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetMemoryWin32HandlePropertiesKHR(VkDevice device, VkFlags, void *handle,
                                                                         void *)
{
    AV_SCOPE("local_vkGetMemoryWin32HandlePropertiesKHR");
    return icd_binding.root().memory_handle_properties(device, handle);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetSemaphoreWin32HandleKHR(
    VkDevice device, const SemaphoreGetWin32HandleRecord *pGetWin32HandleInfo, void **pHandle)
{
    AV_SCOPE("local_vkGetSemaphoreWin32HandleKHR");
    return icd_binding.root().export_semaphore_handle(device, pGetWin32HandleInfo, pHandle);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkImportSemaphoreWin32HandleKHR(
    VkDevice device, const ImportSemaphoreWin32HandleRecord *pImportSemaphoreWin32HandleInfo)
{
    AV_SCOPE("local_vkImportSemaphoreWin32HandleKHR");
    return icd_binding.root().import_semaphore_handle(device, pImportSemaphoreWin32HandleInfo);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                                                    const VkAllocationCallbacks *pAllocator)
{
    AV_SCOPE("local_vkDestroySemaphore");
    icd_binding.root().destroy_semaphore(device, semaphore, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
    VkExternalBufferProperties *pExternalBufferProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceExternalBufferProperties");
    icd_binding.root().external_buffer_properties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
    AV_SCOPE("local_vkGetPhysicalDeviceExternalSemaphoreProperties");
    icd_binding.root().external_semaphore_properties(physicalDevice, pExternalSemaphoreInfo,
                                                     pExternalSemaphoreProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkFlushMappedMemoryRanges(VkDevice device, uint32_t, const VkMappedMemoryRange *)
{
    AV_SCOPE("local_vkFlushMappedMemoryRanges");
    return icd_binding.root().mapped_ranges(device, "vkFlushMappedMemoryRanges");
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t,
                                                                    const VkMappedMemoryRange *)
{
    AV_SCOPE("local_vkInvalidateMappedMemoryRanges");
    return icd_binding.root().mapped_ranges(device, "vkInvalidateMappedMemoryRanges");
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateWin32SurfaceKHR(VkInstance instance,
                                                             const Win32SurfaceCreateRecord *pCreateInfo,
                                                             const VkAllocationCallbacks *, VkSurfaceKHR *pSurface)
{
    AV_SCOPE("local_vkCreateWin32SurfaceKHR");
    return icd_binding.root().presentation().create_surface(instance, pCreateInfo, pSurface);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                                     const VkAllocationCallbacks *)
{
    AV_SCOPE("local_vkDestroySurfaceKHR");
    icd_binding.root().presentation().destroy_surface(instance, surface);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice, uint32_t,
                                                                          VkSurfaceKHR surface, VkBool32 *pSupported)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfaceSupportKHR");
    return icd_binding.root().presentation().surface_support(surface, pSupported);
}

VKAPI_ATTR VkBool32 VKAPI_CALL local_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice, uint32_t)
{
    AV_SCOPE("local_vkGetPhysicalDeviceWin32PresentationSupportKHR");
    return VK_TRUE;
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    return icd_binding.root().presentation().capabilities(surface, pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    return icd_binding.root().presentation().capabilities2(pSurfaceInfo, pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice, VkSurfaceKHR surface,
                                                                          uint32_t *pSurfaceFormatCount,
                                                                          VkSurfaceFormatKHR *pSurfaceFormats)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfaceFormatsKHR");
    return icd_binding.root().presentation().formats(surface, pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, uint32_t *pSurfaceFormatCount,
    VkSurfaceFormat2KHR *pSurfaceFormats)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfaceFormats2KHR");
    return icd_binding.root().presentation().formats2(pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice, VkSurfaceKHR surface,
                                                                               uint32_t *pPresentModeCount,
                                                                               VkPresentModeKHR *pPresentModes)
{
    AV_SCOPE("local_vkGetPhysicalDeviceSurfacePresentModesKHR");
    return icd_binding.root().presentation().present_modes(surface, pPresentModeCount, pPresentModes);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                          const VkAllocationCallbacks *, VkSwapchainKHR *pSwapchain)
{
    AV_SCOPE("local_vkCreateSwapchainKHR");
    return icd_binding.root().presentation().create_swapchain(device, pCreateInfo, pSwapchain);
}

VKAPI_ATTR void VKAPI_CALL local_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                       const VkAllocationCallbacks *)
{
    AV_SCOPE("local_vkDestroySwapchainKHR");
    icd_binding.root().presentation().destroy_swapchain(device, swapchain);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                             uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages)
{
    AV_SCOPE("local_vkGetSwapchainImagesKHR");
    return icd_binding.root().presentation().swapchain_images(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                                           VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
{
    AV_SCOPE("local_vkAcquireNextImageKHR");
    return icd_binding.root().presentation().acquire(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

VKAPI_ATTR VkResult VKAPI_CALL local_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
    AV_SCOPE("local_vkQueuePresentKHR");
    return icd_binding.root().presentation().present(queue, pPresentInfo);
}

}  // namespace av::icd
