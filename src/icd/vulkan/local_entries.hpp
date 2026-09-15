/* The entry points the ICD answers itself rather than through a generated
 * wrapper (contracts/loader_interface.md). The loader calls each through a bare
 * function pointer with Vulkan's arguments and nothing else, so each is the ABI
 * boundary CLAUDE.md permits: one line into the bound IcdRoot, no logic.
 * IcdRoot's table names them. */
#ifndef AV_ICD_LOCAL_ENTRIES_HPP
#define AV_ICD_LOCAL_ENTRIES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion);
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceExtensionProperties(const char *pLayerName,
                                                                            uint32_t *pPropertyCount,
                                                                            VkExtensionProperties *pProperties);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceVersion(uint32_t *pApiVersion);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vk_icdGetPhysicalDeviceProcAddr(VkInstance instance,
                                                                               const char *pName);
/* The loader passes a LUID structure by value: 8 bytes, which the x64 calling
 * conventions carry exactly as a 64-bit integer: LowPart in its low half, as
 * Kmt::adapter_luid answers it. */
VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdEnumerateAdapterPhysicalDevices(VkInstance instance, uint64_t adapterLUID,
                                                                           uint32_t *pPhysicalDeviceCount,
                                                                           VkPhysicalDevice *pPhysicalDevices);
VKAPI_ATTR void VKAPI_CALL local_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceProperties *pProperties);
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                                VkPhysicalDeviceProperties2 *pProperties);
VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                                  VkQueue *pQueue);
VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo,
                                                   VkQueue *pQueue);
VKAPI_ATTR VkResult VKAPI_CALL local_vkAllocateCommandBuffers(VkDevice device,
                                                              const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                              VkCommandBuffer *pCommandBuffers);
VKAPI_ATTR void VKAPI_CALL local_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                      uint32_t commandBufferCount,
                                                      const VkCommandBuffer *pCommandBuffers);
VKAPI_ATTR VkResult VKAPI_CALL local_vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                          const VkCommandBufferBeginInfo *pBeginInfo);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEndCommandBuffer(VkCommandBuffer commandBuffer);
VKAPI_ATTR VkResult VKAPI_CALL local_vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                                          VkCommandBufferResetFlags flags);
VKAPI_ATTR VkResult VKAPI_CALL local_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences,
                                                     VkBool32 waitAll, uint64_t timeout);
VKAPI_ATTR VkResult VKAPI_CALL local_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo,
                                                      uint64_t timeout);
VKAPI_ATTR VkResult VKAPI_CALL local_vkQueueWaitIdle(VkQueue queue);
VKAPI_ATTR VkResult VKAPI_CALL local_vkDeviceWaitIdle(VkDevice device);

}  // namespace av::icd

#endif  // AV_ICD_LOCAL_ENTRIES_HPP
