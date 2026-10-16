/* The entry points the ICD answers itself rather than through a generated
 * wrapper (contracts/loader_interface.md). The loader calls each through a bare
 * function pointer with Vulkan's arguments and nothing else, so each is the ABI
 * boundary CLAUDE.md permits: one line into the bound IcdRoot, no logic.
 * IcdRoot's table names them. */
#ifndef AV_ICD_LOCAL_ENTRIES_HPP
#define AV_ICD_LOCAL_ENTRIES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

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
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vkGetDeviceProcAddr(VkDevice device, const char *pName);
/* The loader passes a LUID structure by value: 8 bytes, which the x64 calling
 * conventions carry exactly as a 64-bit integer: LowPart in its low half, as
 * Kmt::adapter_luid answers it. */
VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdEnumerateAdapterPhysicalDevices(VkInstance instance, uint64_t adapterLUID,
                                                                           uint32_t *pPhysicalDeviceCount,
                                                                           VkPhysicalDevice *pPhysicalDevices);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                          const char *pLayerName,
                                                                          uint32_t *pPropertyCount,
                                                                          VkExtensionProperties *pProperties);
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
VKAPI_ATTR void VKAPI_CALL local_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator);
/* The four that keep the descriptor template table honest (feature 047):
 * each forwards to the generated wrapper, and what it made or destroyed
 * is remembered or forgotten around it. */
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate);
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateDescriptorUpdateTemplateKHR(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate);
VKAPI_ATTR void VKAPI_CALL local_vkDestroyDescriptorUpdateTemplate(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR void VKAPI_CALL local_vkDestroyDescriptorUpdateTemplateKHR(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    const VkAllocationCallbacks *pAllocator);
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
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery,
                                                           uint32_t queryCount, size_t dataSize, void *pData,
                                                           VkDeviceSize stride, VkQueryResultFlags flags);
VKAPI_ATTR VkResult VKAPI_CALL local_vkDeviceWaitIdle(VkDevice device);
/* Not Vulkan's: the probe's forced fault. Device level, so vkGetDeviceProcAddr
 * answers the driver's own pointer: a loader reaches a physical-device or
 * instance function it does not know only through a trampoline of its own,
 * and a loader built without its assembly has none. */
VKAPI_ATTR VkResult VKAPI_CALL local_vkAvForceContextFault(VkDevice device);
/* The NTSTATUS of the last D3DKMT call this driver made (feature 053): what a
 * probe reports when an entry point refused, since every one of the kernel
 * calls behind sharing answers the same VK_ERROR_TOO_MANY_OBJECTS. Device
 * level, as the forced fault is, and for the same reason; the device is
 * checked like any other handle. */
VKAPI_ATTR VkResult VKAPI_CALL local_vkAvLastKernelStatus(VkDevice device, uint32_t *status);
VKAPI_ATTR void VKAPI_CALL local_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator);
/* Every adapter's devices and groups, through the instances it spans. */
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                                                VkPhysicalDevice *pPhysicalDevices);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumeratePhysicalDeviceGroups(
    VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);
VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumeratePhysicalDeviceGroupsKHR(
    VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                                     VkPhysicalDeviceMemoryProperties *pMemoryProperties);
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                                      VkPhysicalDeviceMemoryProperties2 *pMemoryProperties);
VKAPI_ATTR VkResult VKAPI_CALL local_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetMemoryHostPointerPropertiesEXT(
    VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void *pHostPointer,
    VkMemoryHostPointerPropertiesEXT *pMemoryHostPointerProperties);
VKAPI_ATTR void VKAPI_CALL local_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                              const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL local_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                                 VkDeviceSize size, VkMemoryMapFlags flags, void **ppData);
/* Mapped memory is the chunk itself - a host window chunk the host maps too,
 * or the process's own RAM - so there is nothing to unmap, flush or
 * invalidate: the host GPU reads and writes the same bytes. */
VKAPI_ATTR void VKAPI_CALL local_vkUnmapMemory(VkDevice device, VkDeviceMemory memory);
/* The VkMemoryMapInfoKHR shape of the two above (feature 047). Placement is
 * refused rather than ignored: the ICD cannot place a mapping, and a caller
 * that asked for one must not be handed a mapping somewhere else
 * (contracts/placed_mappings.md). */
VKAPI_ATTR VkResult VKAPI_CALL local_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfoKHR *pMemoryMapInfo,
                                                     void **ppData);
VKAPI_ATTR VkResult VKAPI_CALL local_vkUnmapMemory2KHR(VkDevice device,
                                                       const VkMemoryUnmapInfoKHR *pMemoryUnmapInfo);
VKAPI_ATTR VkResult VKAPI_CALL local_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
                                                               const VkMappedMemoryRange *pMemoryRanges);
VKAPI_ATTR VkResult VKAPI_CALL local_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
                                                                    const VkMappedMemoryRange *pMemoryRanges);

/* The Win32 external objects (feature 047, US3). A HANDLE is Windows' own type
 * and no portable header declares it, so the structures arrive as the records
 * beside these declarations and a handle as the opaque pointer it is; the
 * layouts are the same either way (memory_win32_handle_record.hpp). All four
 * are this ICD's own because what a handle names is a row of its own table,
 * shared with the host by fd (research.md §10). */
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetMemoryWin32HandleKHR(VkDevice device,
                                                               const MemoryGetWin32HandleRecord *pGetWin32HandleInfo,
                                                               void                            **pHandle);
/* Which memory types a handle from *outside* may be imported as, which is
 * nothing here: a handle this ICD did not export names no object. */
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetMemoryWin32HandlePropertiesKHR(VkDevice device, VkFlags handleType,
                                                                         void *handle,
                                                                         void *pMemoryWin32HandleProperties);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetSemaphoreWin32HandleKHR(
    VkDevice device, const SemaphoreGetWin32HandleRecord *pGetWin32HandleInfo, void **pHandle);
VKAPI_ATTR VkResult VKAPI_CALL local_vkImportSemaphoreWin32HandleKHR(
    VkDevice device, const ImportSemaphoreWin32HandleRecord *pImportSemaphoreWin32HandleInfo);
/* A semaphore destroyed takes any handle that named it with it. */
VKAPI_ATTR void VKAPI_CALL local_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                                                    const VkAllocationCallbacks *pAllocator);
/* What the host says of its own opaque fd, said in Win32's spelling; every
 * other handle type is answered as absent rather than asked of a host that
 * means something else by it. */
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
    VkExternalBufferProperties *pExternalBufferProperties);
VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties *pExternalSemaphoreProperties);

/* WSI (feature 046). */
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateWin32SurfaceKHR(VkInstance instance,
                                                             const Win32SurfaceCreateRecord *pCreateInfo,
                                                             const VkAllocationCallbacks *pAllocator,
                                                             VkSurfaceKHR *pSurface);
VKAPI_ATTR void VKAPI_CALL local_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                                     const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                                          uint32_t queueFamilyIndex,
                                                                          VkSurfaceKHR surface, VkBool32 *pSupported);
VKAPI_ATTR VkBool32 VKAPI_CALL local_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                                                    uint32_t queueFamilyIndex);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
    VkSurfaceCapabilities2KHR *pSurfaceCapabilities);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                                          VkSurfaceKHR surface,
                                                                          uint32_t *pSurfaceFormatCount,
                                                                          VkSurfaceFormatKHR *pSurfaceFormats);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfaceFormats2KHR(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
    uint32_t *pSurfaceFormatCount, VkSurfaceFormat2KHR *pSurfaceFormats);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice,
                                                                               VkSurfaceKHR surface,
                                                                               uint32_t *pPresentModeCount,
                                                                               VkPresentModeKHR *pPresentModes);
VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                          const VkAllocationCallbacks *pAllocator,
                                                          VkSwapchainKHR *pSwapchain);
VKAPI_ATTR void VKAPI_CALL local_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                       const VkAllocationCallbacks *pAllocator);
VKAPI_ATTR VkResult VKAPI_CALL local_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                             uint32_t *pSwapchainImageCount,
                                                             VkImage *pSwapchainImages);
VKAPI_ATTR VkResult VKAPI_CALL local_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                                           VkSemaphore semaphore, VkFence fence,
                                                           uint32_t *pImageIndex);
VKAPI_ATTR VkResult VKAPI_CALL local_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo);

}  // namespace av::icd

#endif  // AV_ICD_LOCAL_ENTRIES_HPP
