#include "icd/vulkan/local_entries.hpp"

#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

namespace av::icd {

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion)
{ return icd_binding.root().negotiate(pSupportedVersion); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{ return icd_binding.root().create_instance(pCreateInfo, pAllocator, pInstance); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceExtensionProperties(const char *pLayerName,
                                                                            uint32_t *pPropertyCount,
                                                                            VkExtensionProperties *pProperties)
{ return icd_binding.root().enumerate_instance_extensions(pLayerName, pPropertyCount, pProperties); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceVersion(uint32_t *pApiVersion)
{ return icd_binding.root().enumerate_instance_version(pApiVersion); }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vk_icdGetPhysicalDeviceProcAddr(VkInstance, const char *pName)
{ return icd_binding.root().physical_device_proc_addr(pName); }

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdEnumerateAdapterPhysicalDevices(VkInstance instance, uint64_t adapterLUID,
                                                                           uint32_t *pPhysicalDeviceCount,
                                                                           VkPhysicalDevice *pPhysicalDevices)
{ return icd_binding.root().enumerate_adapter_physical_devices(instance, adapterLUID, pPhysicalDeviceCount,
                                                                pPhysicalDevices); }

VKAPI_ATTR void VKAPI_CALL local_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{ icd_binding.root().destroy_device(device, pAllocator); }

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceProperties *pProperties)
{ icd_binding.root().physical_device_properties(physicalDevice, pProperties); }

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                                VkPhysicalDeviceProperties2 *pProperties)
{ icd_binding.root().physical_device_properties2(physicalDevice, pProperties); }

VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                                  VkQueue *pQueue)
{ icd_binding.root().device_queue(device, queueFamilyIndex, queueIndex, pQueue); }

VKAPI_ATTR void VKAPI_CALL local_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo,
                                                   VkQueue *pQueue)
{ icd_binding.root().device_queue2(device, pQueueInfo, pQueue); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkAllocateCommandBuffers(VkDevice device,
                                                              const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                              VkCommandBuffer *pCommandBuffers)
{ return icd_binding.root().allocate_command_buffers(device, pAllocateInfo, pCommandBuffers); }

VKAPI_ATTR void VKAPI_CALL local_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                      uint32_t commandBufferCount,
                                                      const VkCommandBuffer *pCommandBuffers)
{ icd_binding.root().free_command_buffers(device, commandPool, commandBufferCount, pCommandBuffers); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                          const VkCommandBufferBeginInfo *pBeginInfo)
{ return icd_binding.root().begin_command_buffer(commandBuffer, pBeginInfo); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{ return icd_binding.root().end_command_buffer(commandBuffer); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                                          VkCommandBufferResetFlags flags)
{ return icd_binding.root().reset_command_buffer(commandBuffer, flags); }

}  // namespace av::icd
