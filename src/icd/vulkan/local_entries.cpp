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

}  // namespace av::icd
