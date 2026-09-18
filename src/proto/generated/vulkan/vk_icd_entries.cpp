
#include "vk_icd_entries.h"

#include "core/trace/function_scope.hpp"
#include "proto/icd_connection.hpp"
#include "proto/icd_handle_registry.hpp"
#include "vk_dispatch_client.h"

namespace av::vkicd::generated {
namespace {

// How many raw server handles a wrapper stages on its stack while
// wrapping an array the application asked for. A request for more is
// refused with VK_ERROR_OUT_OF_HOST_MEMORY rather than allocated.
constexpr size_t kMaxStagedHandles = 1024;

}  // namespace

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyInstance");
    if (instance == VK_NULL_HANDLE) {
        return;
    }
    auto* conn = ::av::icd::connection_of(instance);
    ::av::vkproto::rvk_vkDestroyInstance(conn->transport(), instance, pAllocator);
    ::av::icd::destroy_wrapper(instance);
    ::av::icd::release_connection(conn);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDevice");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDevice(conn->transport(), device, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    AV_SCOPE("vkGetDeviceQueue");
    auto* conn = ::av::icd::connection_of(device);
    uint64_t server_pQueue = 0;
    ::av::vkproto::rvk_vkGetDeviceQueue(conn->transport(), device, queueFamilyIndex, queueIndex, &server_pQueue);
    *pQueue = ::av::icd::wrap_server_handle<VkQueue>(server_pQueue, conn);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    AV_SCOPE("vkEnumeratePhysicalDevices");
    auto* conn = ::av::icd::connection_of(instance);
    if (pPhysicalDevices == nullptr) {
        return ::av::vkproto::rvk_vkEnumeratePhysicalDevices(conn->transport(), instance, pPhysicalDeviceCount, nullptr);
    }
    if (static_cast<size_t>(*pPhysicalDeviceCount) > kMaxStagedHandles) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t server_pPhysicalDevices[kMaxStagedHandles] = {};
    const VkResult rvk_rc = ::av::vkproto::rvk_vkEnumeratePhysicalDevices(conn->transport(), instance, pPhysicalDeviceCount, server_pPhysicalDevices);
    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {
        return rvk_rc;
    }
    for (uint32_t k = 0; k < *pPhysicalDeviceCount; ++k) {
        pPhysicalDevices[k] = ::av::icd::wrap_server_handle<VkPhysicalDevice>(server_pPhysicalDevices[k], conn);
    }
    return rvk_rc;
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceProperties2(conn->transport(), physicalDevice, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    AV_SCOPE("vkCreateInstance");
    auto* conn = ::av::icd::acquire_default_connection();
    if (conn == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // 64 bits wide because that is the wire's width. Holding the
    // server's handle in a VkInstance would narrow it on a 32-bit
    // client and send back a handle no object is at.
    uint64_t server_instance = 0;
    const VkResult rvk_rc = ::av::vkproto::rvk_vkCreateInstance(conn->transport(), pCreateInfo, pAllocator, &server_instance);
    if (rvk_rc != VK_SUCCESS) {
        ::av::icd::release_connection(conn);
        return rvk_rc;
    }
    *pInstance = ::av::icd::wrap_server_handle<VkInstance>(server_instance, conn);
    return rvk_rc;
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    AV_SCOPE("vkCreateDevice");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    uint64_t server_pDevice = 0;
    const VkResult rvk_rc = ::av::vkproto::rvk_vkCreateDevice(conn->transport(), physicalDevice, pCreateInfo, pAllocator, &server_pDevice);
    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {
        return rvk_rc;
    }
    *pDevice = ::av::icd::wrap_server_handle<VkDevice>(server_pDevice, conn);
    return rvk_rc;
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceProperties(conn->transport(), physicalDevice, pProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    AV_SCOPE("vkGetPhysicalDeviceFeatures");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFeatures(conn->transport(), physicalDevice, pFeatures);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceFormatProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFormatProperties(conn->transport(), physicalDevice, format, pFormatProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceImageFormatProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceImageFormatProperties(conn->transport(), physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    AV_SCOPE("vkGetPhysicalDeviceMemoryProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(conn->transport(), physicalDevice, pMemoryProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    AV_SCOPE("vkGetPhysicalDeviceQueueFamilyProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties(conn->transport(), physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceSparseImageFormatProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceSparseImageFormatProperties(conn->transport(), physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    AV_SCOPE("vkEnumerateDeviceExtensionProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkEnumerateDeviceExtensionProperties(conn->transport(), physicalDevice, pLayerName, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    AV_SCOPE("vkEnumerateDeviceLayerProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkEnumerateDeviceLayerProperties(conn->transport(), physicalDevice, pPropertyCount, pProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("vkGetPhysicalDeviceFeatures2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFeatures2(conn->transport(), physicalDevice, pFeatures);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceFormatProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFormatProperties2(conn->transport(), physicalDevice, format, pFormatProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceImageFormatProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceImageFormatProperties2(conn->transport(), physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("vkGetPhysicalDeviceMemoryProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceMemoryProperties2(conn->transport(), physicalDevice, pMemoryProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("vkGetPhysicalDeviceQueueFamilyProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties2(conn->transport(), physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceSparseImageFormatProperties2");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceSparseImageFormatProperties2(conn->transport(), physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkDeviceWaitIdle(VkDevice device) {
    AV_SCOPE("vkDeviceWaitIdle");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkDeviceWaitIdle(conn->transport(), device);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkQueueWaitIdle(VkQueue queue) {
    AV_SCOPE("vkQueueWaitIdle");
    auto* conn = ::av::icd::connection_of(queue);
    return ::av::vkproto::rvk_vkQueueWaitIdle(conn->transport(), queue);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    AV_SCOPE("vkAllocateMemory");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkAllocateMemory(conn->transport(), device, pAllocateInfo, pAllocator, pMemory);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkFreeMemory");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkFreeMemory(conn->transport(), device, memory, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("vkBindBufferMemory");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindBufferMemory(conn->transport(), device, buffer, memory, memoryOffset);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("vkBindImageMemory");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindImageMemory(conn->transport(), device, image, memory, memoryOffset);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("vkGetBufferMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetBufferMemoryRequirements(conn->transport(), device, buffer, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("vkGetImageMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageMemoryRequirements(conn->transport(), device, image, pMemoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    AV_SCOPE("vkCreateBuffer");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateBuffer(conn->transport(), device, pCreateInfo, pAllocator, pBuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyBuffer");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyBuffer(conn->transport(), device, buffer, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    AV_SCOPE("vkCreateImage");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateImage(conn->transport(), device, pCreateInfo, pAllocator, pImage);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyImage");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyImage(conn->transport(), device, image, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    AV_SCOPE("vkCreateImageView");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateImageView(conn->transport(), device, pCreateInfo, pAllocator, pView);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyImageView");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyImageView(conn->transport(), device, imageView, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    AV_SCOPE("vkCreateSampler");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateSampler(conn->transport(), device, pCreateInfo, pAllocator, pSampler);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroySampler");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroySampler(conn->transport(), device, sampler, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    AV_SCOPE("vkCreateSemaphore");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateSemaphore(conn->transport(), device, pCreateInfo, pAllocator, pSemaphore);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroySemaphore");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroySemaphore(conn->transport(), device, semaphore, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    AV_SCOPE("vkCreateFence");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateFence(conn->transport(), device, pCreateInfo, pAllocator, pFence);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyFence");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyFence(conn->transport(), device, fence, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    AV_SCOPE("vkResetFences");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkResetFences(conn->transport(), device, fenceCount, pFences);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    AV_SCOPE("vkWaitForFences");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkWaitForFences(conn->transport(), device, fenceCount, pFences, waitAll, timeout);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetFenceStatus(VkDevice device, VkFence fence) {
    AV_SCOPE("vkGetFenceStatus");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetFenceStatus(conn->transport(), device, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    AV_SCOPE("vkCreateCommandPool");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateCommandPool(conn->transport(), device, pCreateInfo, pAllocator, pCommandPool);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyCommandPool");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyCommandPool(conn->transport(), device, commandPool, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    AV_SCOPE("vkResetCommandPool");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkResetCommandPool(conn->transport(), device, commandPool, flags);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    AV_SCOPE("vkBeginCommandBuffer");
    return ::av::vkproto::rvk_vkBeginCommandBuffer(::av::icd::record_target(commandBuffer), commandBuffer, pBeginInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    AV_SCOPE("vkEndCommandBuffer");
    return ::av::vkproto::rvk_vkEndCommandBuffer(::av::icd::record_target(commandBuffer), commandBuffer);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    AV_SCOPE("vkResetCommandBuffer");
    auto* conn = ::av::icd::connection_of(commandBuffer);
    return ::av::vkproto::rvk_vkResetCommandBuffer(conn->transport(), commandBuffer, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyShaderModule");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyShaderModule(conn->transport(), device, shaderModule, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    AV_SCOPE("vkCreateShaderModule");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateShaderModule(conn->transport(), device, pCreateInfo, pAllocator, pShaderModule);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    AV_SCOPE("vkCreatePipelineLayout");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreatePipelineLayout(conn->transport(), device, pCreateInfo, pAllocator, pPipelineLayout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyPipelineLayout");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyPipelineLayout(conn->transport(), device, pipelineLayout, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyPipeline");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyPipeline(conn->transport(), device, pipeline, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    AV_SCOPE("vkCreatePipelineCache");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreatePipelineCache(conn->transport(), device, pCreateInfo, pAllocator, pPipelineCache);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyPipelineCache");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyPipelineCache(conn->transport(), device, pipelineCache, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("vkCreateRenderPass");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateRenderPass(conn->transport(), device, pCreateInfo, pAllocator, pRenderPass);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyRenderPass");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyRenderPass(conn->transport(), device, renderPass, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    AV_SCOPE("vkCreateFramebuffer");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateFramebuffer(conn->transport(), device, pCreateInfo, pAllocator, pFramebuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyFramebuffer");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyFramebuffer(conn->transport(), device, framebuffer, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    AV_SCOPE("vkCreateDescriptorSetLayout");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateDescriptorSetLayout(conn->transport(), device, pCreateInfo, pAllocator, pSetLayout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDescriptorSetLayout");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDescriptorSetLayout(conn->transport(), device, descriptorSetLayout, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    AV_SCOPE("vkCreateDescriptorPool");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateDescriptorPool(conn->transport(), device, pCreateInfo, pAllocator, pDescriptorPool);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDescriptorPool");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDescriptorPool(conn->transport(), device, descriptorPool, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("vkFreeCommandBuffers");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkFreeCommandBuffers(conn->transport(), device, commandPool, commandBufferCount, pCommandBuffers);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    AV_SCOPE("vkQueueSubmit");
    auto* conn = ::av::icd::connection_of(queue);
    return ::av::vkproto::rvk_vkQueueSubmit(conn->transport(), queue, submitCount, pSubmits, fence);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    AV_SCOPE("vkCmdBindPipeline");
    ::av::vkproto::rvk_vkCmdBindPipeline(::av::icd::record_target(commandBuffer), commandBuffer, pipelineBindPoint, pipeline);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("vkCmdSetViewport");
    ::av::vkproto::rvk_vkCmdSetViewport(::av::icd::record_target(commandBuffer), commandBuffer, firstViewport, viewportCount, pViewports);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("vkCmdSetScissor");
    ::av::vkproto::rvk_vkCmdSetScissor(::av::icd::record_target(commandBuffer), commandBuffer, firstScissor, scissorCount, pScissors);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    AV_SCOPE("vkCmdBindVertexBuffers");
    ::av::vkproto::rvk_vkCmdBindVertexBuffers(::av::icd::record_target(commandBuffer), commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    AV_SCOPE("vkCmdBindIndexBuffer");
    ::av::vkproto::rvk_vkCmdBindIndexBuffer(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, indexType);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    AV_SCOPE("vkCmdBindDescriptorSets");
    ::av::vkproto::rvk_vkCmdBindDescriptorSets(::av::icd::record_target(commandBuffer), commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    AV_SCOPE("vkCmdDraw");
    ::av::vkproto::rvk_vkCmdDraw(::av::icd::record_target(commandBuffer), commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    AV_SCOPE("vkCmdDrawIndexed");
    ::av::vkproto::rvk_vkCmdDrawIndexed(::av::icd::record_target(commandBuffer), commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("vkCmdPipelineBarrier");
    ::av::vkproto::rvk_vkCmdPipelineBarrier(::av::icd::record_target(commandBuffer), commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    AV_SCOPE("vkCmdCopyBuffer");
    ::av::vkproto::rvk_vkCmdCopyBuffer(::av::icd::record_target(commandBuffer), commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    AV_SCOPE("vkCmdBeginRenderPass");
    ::av::vkproto::rvk_vkCmdBeginRenderPass(::av::icd::record_target(commandBuffer), commandBuffer, pRenderPassBegin, contents);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    AV_SCOPE("vkCmdEndRenderPass");
    ::av::vkproto::rvk_vkCmdEndRenderPass(::av::icd::record_target(commandBuffer), commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    AV_SCOPE("vkCmdNextSubpass");
    ::av::vkproto::rvk_vkCmdNextSubpass(::av::icd::record_target(commandBuffer), commandBuffer, contents);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    AV_SCOPE("vkCmdPushConstants");
    ::av::vkproto::rvk_vkCmdPushConstants(::av::icd::record_target(commandBuffer), commandBuffer, layout, stageFlags, offset, size, pValues);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("vkCreateGraphicsPipelines");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateGraphicsPipelines(conn->transport(), device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    AV_SCOPE("vkUpdateDescriptorSets");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkUpdateDescriptorSets(conn->transport(), device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("vkAllocateCommandBuffers");
    auto* conn = ::av::icd::connection_of(device);
    if (static_cast<size_t>(pAllocateInfo->commandBufferCount) > kMaxStagedHandles) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    uint64_t server_pCommandBuffers[kMaxStagedHandles] = {};
    const VkResult rvk_rc = ::av::vkproto::rvk_vkAllocateCommandBuffers(conn->transport(), device, pAllocateInfo, server_pCommandBuffers);
    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {
        return rvk_rc;
    }
    for (uint32_t k = 0; k < static_cast<uint32_t>(pAllocateInfo->commandBufferCount); ++k) {
        pCommandBuffers[k] = ::av::icd::wrap_server_handle<VkCommandBuffer>(server_pCommandBuffers[k], conn);
    }
    return rvk_rc;
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("vkAllocateDescriptorSets");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkAllocateDescriptorSets(conn->transport(), device, pAllocateInfo, pDescriptorSets);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("vkFreeDescriptorSets");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkFreeDescriptorSets(conn->transport(), device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("vkCreateComputePipelines");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateComputePipelines(conn->transport(), device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    AV_SCOPE("vkResetDescriptorPool");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkResetDescriptorPool(conn->transport(), device, descriptorPool, flags);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    AV_SCOPE("vkCreateEvent");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateEvent(conn->transport(), device, pCreateInfo, pAllocator, pEvent);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyEvent");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyEvent(conn->transport(), device, event, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetEventStatus(VkDevice device, VkEvent event) {
    AV_SCOPE("vkGetEventStatus");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetEventStatus(conn->transport(), device, event);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkSetEvent(VkDevice device, VkEvent event) {
    AV_SCOPE("vkSetEvent");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkSetEvent(conn->transport(), device, event);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkResetEvent(VkDevice device, VkEvent event) {
    AV_SCOPE("vkResetEvent");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkResetEvent(conn->transport(), device, event);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("vkCmdSetEvent");
    ::av::vkproto::rvk_vkCmdSetEvent(::av::icd::record_target(commandBuffer), commandBuffer, event, stageMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("vkCmdResetEvent");
    ::av::vkproto::rvk_vkCmdResetEvent(::av::icd::record_target(commandBuffer), commandBuffer, event, stageMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("vkCmdWaitEvents");
    ::av::vkproto::rvk_vkCmdWaitEvents(::av::icd::record_target(commandBuffer), commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    AV_SCOPE("vkCmdCopyImage");
    ::av::vkproto::rvk_vkCmdCopyImage(::av::icd::record_target(commandBuffer), commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("vkCmdCopyBufferToImage");
    ::av::vkproto::rvk_vkCmdCopyBufferToImage(::av::icd::record_target(commandBuffer), commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("vkCmdCopyImageToBuffer");
    ::av::vkproto::rvk_vkCmdCopyImageToBuffer(::av::icd::record_target(commandBuffer), commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    AV_SCOPE("vkCmdBlitImage");
    ::av::vkproto::rvk_vkCmdBlitImage(::av::icd::record_target(commandBuffer), commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("vkCmdClearColorImage");
    ::av::vkproto::rvk_vkCmdClearColorImage(::av::icd::record_target(commandBuffer), commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("vkCmdClearDepthStencilImage");
    ::av::vkproto::rvk_vkCmdClearDepthStencilImage(::av::icd::record_target(commandBuffer), commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    AV_SCOPE("vkCmdClearAttachments");
    ::av::vkproto::rvk_vkCmdClearAttachments(::av::icd::record_target(commandBuffer), commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    AV_SCOPE("vkCmdFillBuffer");
    ::av::vkproto::rvk_vkCmdFillBuffer(::av::icd::record_target(commandBuffer), commandBuffer, dstBuffer, dstOffset, size, data);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    AV_SCOPE("vkCmdUpdateBuffer");
    ::av::vkproto::rvk_vkCmdUpdateBuffer(::av::icd::record_target(commandBuffer), commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    AV_SCOPE("vkCmdSetLineWidth");
    ::av::vkproto::rvk_vkCmdSetLineWidth(::av::icd::record_target(commandBuffer), commandBuffer, lineWidth);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    AV_SCOPE("vkCmdSetDepthBias");
    ::av::vkproto::rvk_vkCmdSetDepthBias(::av::icd::record_target(commandBuffer), commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float* blendConstants) {
    AV_SCOPE("vkCmdSetBlendConstants");
    ::av::vkproto::rvk_vkCmdSetBlendConstants(::av::icd::record_target(commandBuffer), commandBuffer, blendConstants);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    AV_SCOPE("vkCmdSetDepthBounds");
    ::av::vkproto::rvk_vkCmdSetDepthBounds(::av::icd::record_target(commandBuffer), commandBuffer, minDepthBounds, maxDepthBounds);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    AV_SCOPE("vkCmdSetStencilCompareMask");
    ::av::vkproto::rvk_vkCmdSetStencilCompareMask(::av::icd::record_target(commandBuffer), commandBuffer, faceMask, compareMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    AV_SCOPE("vkCmdSetStencilWriteMask");
    ::av::vkproto::rvk_vkCmdSetStencilWriteMask(::av::icd::record_target(commandBuffer), commandBuffer, faceMask, writeMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    AV_SCOPE("vkCmdSetStencilReference");
    ::av::vkproto::rvk_vkCmdSetStencilReference(::av::icd::record_target(commandBuffer), commandBuffer, faceMask, reference);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("vkCmdDispatch");
    ::av::vkproto::rvk_vkCmdDispatch(::av::icd::record_target(commandBuffer), commandBuffer, groupCountX, groupCountY, groupCountZ);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    AV_SCOPE("vkCmdDispatchIndirect");
    ::av::vkproto::rvk_vkCmdDispatchIndirect(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndirect");
    ::av::vkproto::rvk_vkCmdDrawIndirect(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, drawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndexedIndirect");
    ::av::vkproto::rvk_vkCmdDrawIndexedIndirect(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, drawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("vkCmdExecuteCommands");
    ::av::vkproto::rvk_vkCmdExecuteCommands(::av::icd::record_target(commandBuffer), commandBuffer, commandBufferCount, pCommandBuffers);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    AV_SCOPE("vkAcquireNextImage2KHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkAcquireNextImage2KHR(conn->transport(), device, pAcquireInfo, pImageIndex);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("vkBindBufferMemory2");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindBufferMemory2(conn->transport(), device, bindInfoCount, pBindInfos);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("vkBindImageMemory2");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindImageMemory2(conn->transport(), device, bindInfoCount, pBindInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    AV_SCOPE("vkCmdBeginQuery");
    ::av::vkproto::rvk_vkCmdBeginQuery(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, query, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("vkCmdBeginRenderPass2");
    ::av::vkproto::rvk_vkCmdBeginRenderPass2(::av::icd::record_target(commandBuffer), commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("vkCmdBeginRendering");
    ::av::vkproto::rvk_vkCmdBeginRendering(::av::icd::record_target(commandBuffer), commandBuffer, pRenderingInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("vkCmdBindVertexBuffers2");
    ::av::vkproto::rvk_vkCmdBindVertexBuffers2(::av::icd::record_target(commandBuffer), commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("vkCmdBlitImage2");
    ::av::vkproto::rvk_vkCmdBlitImage2(::av::icd::record_target(commandBuffer), commandBuffer, pBlitImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("vkCmdCopyBuffer2");
    ::av::vkproto::rvk_vkCmdCopyBuffer2(::av::icd::record_target(commandBuffer), commandBuffer, pCopyBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("vkCmdCopyBufferToImage2");
    ::av::vkproto::rvk_vkCmdCopyBufferToImage2(::av::icd::record_target(commandBuffer), commandBuffer, pCopyBufferToImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("vkCmdCopyImage2");
    ::av::vkproto::rvk_vkCmdCopyImage2(::av::icd::record_target(commandBuffer), commandBuffer, pCopyImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("vkCmdCopyImageToBuffer2");
    ::av::vkproto::rvk_vkCmdCopyImageToBuffer2(::av::icd::record_target(commandBuffer), commandBuffer, pCopyImageToBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("vkCmdCopyQueryPoolResults");
    ::av::vkproto::rvk_vkCmdCopyQueryPoolResults(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("vkCmdDispatchBase");
    ::av::vkproto::rvk_vkCmdDispatchBase(::av::icd::record_target(commandBuffer), commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndexedIndirectCount");
    ::av::vkproto::rvk_vkCmdDrawIndexedIndirectCount(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndirectCount");
    ::av::vkproto::rvk_vkCmdDrawIndirectCount(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("vkCmdEndQuery");
    ::av::vkproto::rvk_vkCmdEndQuery(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, query);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("vkCmdEndRenderPass2");
    ::av::vkproto::rvk_vkCmdEndRenderPass2(::av::icd::record_target(commandBuffer), commandBuffer, pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndRendering(VkCommandBuffer commandBuffer) {
    AV_SCOPE("vkCmdEndRendering");
    ::av::vkproto::rvk_vkCmdEndRendering(::av::icd::record_target(commandBuffer), commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("vkCmdNextSubpass2");
    ::av::vkproto::rvk_vkCmdNextSubpass2(::av::icd::record_target(commandBuffer), commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("vkCmdPipelineBarrier2");
    ::av::vkproto::rvk_vkCmdPipelineBarrier2(::av::icd::record_target(commandBuffer), commandBuffer, pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("vkCmdResetEvent2");
    ::av::vkproto::rvk_vkCmdResetEvent2(::av::icd::record_target(commandBuffer), commandBuffer, event, stageMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("vkCmdResetQueryPool");
    ::av::vkproto::rvk_vkCmdResetQueryPool(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, firstQuery, queryCount);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    AV_SCOPE("vkCmdResolveImage");
    ::av::vkproto::rvk_vkCmdResolveImage(::av::icd::record_target(commandBuffer), commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("vkCmdResolveImage2");
    ::av::vkproto::rvk_vkCmdResolveImage2(::av::icd::record_target(commandBuffer), commandBuffer, pResolveImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("vkCmdSetCullMode");
    ::av::vkproto::rvk_vkCmdSetCullMode(::av::icd::record_target(commandBuffer), commandBuffer, cullMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("vkCmdSetDepthBiasEnable");
    ::av::vkproto::rvk_vkCmdSetDepthBiasEnable(::av::icd::record_target(commandBuffer), commandBuffer, depthBiasEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("vkCmdSetDepthBoundsTestEnable");
    ::av::vkproto::rvk_vkCmdSetDepthBoundsTestEnable(::av::icd::record_target(commandBuffer), commandBuffer, depthBoundsTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("vkCmdSetDepthCompareOp");
    ::av::vkproto::rvk_vkCmdSetDepthCompareOp(::av::icd::record_target(commandBuffer), commandBuffer, depthCompareOp);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("vkCmdSetDepthTestEnable");
    ::av::vkproto::rvk_vkCmdSetDepthTestEnable(::av::icd::record_target(commandBuffer), commandBuffer, depthTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("vkCmdSetDepthWriteEnable");
    ::av::vkproto::rvk_vkCmdSetDepthWriteEnable(::av::icd::record_target(commandBuffer), commandBuffer, depthWriteEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("vkCmdSetDeviceMask");
    ::av::vkproto::rvk_vkCmdSetDeviceMask(::av::icd::record_target(commandBuffer), commandBuffer, deviceMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("vkCmdSetEvent2");
    ::av::vkproto::rvk_vkCmdSetEvent2(::av::icd::record_target(commandBuffer), commandBuffer, event, pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("vkCmdSetFrontFace");
    ::av::vkproto::rvk_vkCmdSetFrontFace(::av::icd::record_target(commandBuffer), commandBuffer, frontFace);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("vkCmdSetPrimitiveRestartEnable");
    ::av::vkproto::rvk_vkCmdSetPrimitiveRestartEnable(::av::icd::record_target(commandBuffer), commandBuffer, primitiveRestartEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("vkCmdSetPrimitiveTopology");
    ::av::vkproto::rvk_vkCmdSetPrimitiveTopology(::av::icd::record_target(commandBuffer), commandBuffer, primitiveTopology);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("vkCmdSetRasterizerDiscardEnable");
    ::av::vkproto::rvk_vkCmdSetRasterizerDiscardEnable(::av::icd::record_target(commandBuffer), commandBuffer, rasterizerDiscardEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("vkCmdSetScissorWithCount");
    ::av::vkproto::rvk_vkCmdSetScissorWithCount(::av::icd::record_target(commandBuffer), commandBuffer, scissorCount, pScissors);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("vkCmdSetStencilOp");
    ::av::vkproto::rvk_vkCmdSetStencilOp(::av::icd::record_target(commandBuffer), commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("vkCmdSetStencilTestEnable");
    ::av::vkproto::rvk_vkCmdSetStencilTestEnable(::av::icd::record_target(commandBuffer), commandBuffer, stencilTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("vkCmdSetViewportWithCount");
    ::av::vkproto::rvk_vkCmdSetViewportWithCount(::av::icd::record_target(commandBuffer), commandBuffer, viewportCount, pViewports);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("vkCmdWaitEvents2");
    ::av::vkproto::rvk_vkCmdWaitEvents2(::av::icd::record_target(commandBuffer), commandBuffer, eventCount, pEvents, pDependencyInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("vkCmdWriteTimestamp");
    ::av::vkproto::rvk_vkCmdWriteTimestamp(::av::icd::record_target(commandBuffer), commandBuffer, pipelineStage, queryPool, query);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("vkCmdWriteTimestamp2");
    ::av::vkproto::rvk_vkCmdWriteTimestamp2(::av::icd::record_target(commandBuffer), commandBuffer, stage, queryPool, query);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    AV_SCOPE("vkCreateBufferView");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateBufferView(conn->transport(), device, pCreateInfo, pAllocator, pView);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("vkCreateDescriptorUpdateTemplate");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateDescriptorUpdateTemplate(conn->transport(), device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    AV_SCOPE("vkCreatePrivateDataSlot");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreatePrivateDataSlot(conn->transport(), device, pCreateInfo, pAllocator, pPrivateDataSlot);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    AV_SCOPE("vkCreateQueryPool");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateQueryPool(conn->transport(), device, pCreateInfo, pAllocator, pQueryPool);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("vkCreateRenderPass2");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateRenderPass2(conn->transport(), device, pCreateInfo, pAllocator, pRenderPass);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("vkCreateSamplerYcbcrConversion");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateSamplerYcbcrConversion(conn->transport(), device, pCreateInfo, pAllocator, pYcbcrConversion);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyBufferView");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyBufferView(conn->transport(), device, bufferView, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDescriptorUpdateTemplate");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDescriptorUpdateTemplate(conn->transport(), device, descriptorUpdateTemplate, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyPrivateDataSlot");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyPrivateDataSlot(conn->transport(), device, privateDataSlot, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyQueryPool");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyQueryPool(conn->transport(), device, queryPool, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroySamplerYcbcrConversion");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroySamplerYcbcrConversion(conn->transport(), device, ycbcrConversion, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("vkEnumeratePhysicalDeviceGroups");
    auto* conn = ::av::icd::connection_of(instance);
    if (pPhysicalDeviceGroupProperties == nullptr) {
        return ::av::vkproto::rvk_vkEnumeratePhysicalDeviceGroups(conn->transport(), instance, pPhysicalDeviceGroupCount, nullptr);
    }
    const VkResult rvk_rc = ::av::vkproto::rvk_vkEnumeratePhysicalDeviceGroups(conn->transport(), instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {
        return rvk_rc;
    }
    for (uint32_t k = 0; k < *pPhysicalDeviceGroupCount; ++k) {
        for (uint32_t j = 0; j < pPhysicalDeviceGroupProperties[k].physicalDeviceCount; ++j) {
            pPhysicalDeviceGroupProperties[k].physicalDevices[j] = ::av::icd::wrap_server_handle<VkPhysicalDevice>(reinterpret_cast<uint64_t>(pPhysicalDeviceGroupProperties[k].physicalDevices[j]), conn);
            if (pPhysicalDeviceGroupProperties[k].physicalDevices[j] == VK_NULL_HANDLE) {
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }
    return rvk_rc;
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("vkFlushMappedMemoryRanges");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkFlushMappedMemoryRanges(conn->transport(), device, memoryRangeCount, pMemoryRanges);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL Wrap_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("vkGetBufferDeviceAddress");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetBufferDeviceAddress(conn->transport(), device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetBufferMemoryRequirements2");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetBufferMemoryRequirements2(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR uint64_t VKAPI_CALL Wrap_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("vkGetBufferOpaqueCaptureAddress");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetBufferOpaqueCaptureAddress(conn->transport(), device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("vkGetDescriptorSetLayoutSupport");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDescriptorSetLayoutSupport(conn->transport(), device, pCreateInfo, pSupport);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetDeviceBufferMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceBufferMemoryRequirements(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("vkGetDeviceGroupPeerMemoryFeatures");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceGroupPeerMemoryFeatures(conn->transport(), device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    AV_SCOPE("vkGetDeviceGroupPresentCapabilitiesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeviceGroupPresentCapabilitiesKHR(conn->transport(), device, pDeviceGroupPresentCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    AV_SCOPE("vkGetDeviceGroupSurfacePresentModesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeviceGroupSurfacePresentModesKHR(conn->transport(), device, surface, pModes);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetDeviceImageMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceImageMemoryRequirements(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("vkGetDeviceImageSparseMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceImageSparseMemoryRequirements(conn->transport(), device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    AV_SCOPE("vkGetDeviceMemoryCommitment");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceMemoryCommitment(conn->transport(), device, memory, pCommittedMemoryInBytes);
}

VKAPI_ATTR uint64_t VKAPI_CALL Wrap_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("vkGetDeviceMemoryOpaqueCaptureAddress");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeviceMemoryOpaqueCaptureAddress(conn->transport(), device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) {
    AV_SCOPE("vkGetDeviceQueue2");
    auto* conn = ::av::icd::connection_of(device);
    uint64_t server_pQueue = 0;
    ::av::vkproto::rvk_vkGetDeviceQueue2(conn->transport(), device, pQueueInfo, &server_pQueue);
    *pQueue = ::av::icd::wrap_server_handle<VkQueue>(server_pQueue, conn);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetImageMemoryRequirements2");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageMemoryRequirements2(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    AV_SCOPE("vkGetImageSparseMemoryRequirements");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSparseMemoryRequirements(conn->transport(), device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("vkGetImageSparseMemoryRequirements2");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSparseMemoryRequirements2(conn->transport(), device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    AV_SCOPE("vkGetImageSubresourceLayout");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSubresourceLayout(conn->transport(), device, image, pSubresource, pLayout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalBufferProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalBufferProperties(conn->transport(), physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalFenceProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalFenceProperties(conn->transport(), physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalSemaphoreProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalSemaphoreProperties(conn->transport(), physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    AV_SCOPE("vkGetPhysicalDevicePresentRectanglesKHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDevicePresentRectanglesKHR(conn->transport(), physicalDevice, surface, pRectCount, pRects);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    AV_SCOPE("vkGetPhysicalDeviceToolProperties");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceToolProperties(conn->transport(), physicalDevice, pToolCount, pToolProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    AV_SCOPE("vkGetPrivateData");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetPrivateData(conn->transport(), device, objectType, objectHandle, privateDataSlot, pData);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("vkGetQueryPoolResults");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetQueryPoolResults(conn->transport(), device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    AV_SCOPE("vkGetRenderAreaGranularity");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetRenderAreaGranularity(conn->transport(), device, renderPass, pGranularity);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("vkGetSemaphoreCounterValue");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetSemaphoreCounterValue(conn->transport(), device, semaphore, pValue);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("vkInvalidateMappedMemoryRanges");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkInvalidateMappedMemoryRanges(conn->transport(), device, memoryRangeCount, pMemoryRanges);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    AV_SCOPE("vkMergePipelineCaches");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkMergePipelineCaches(conn->transport(), device, dstCache, srcCacheCount, pSrcCaches);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    AV_SCOPE("vkQueueBindSparse");
    auto* conn = ::av::icd::connection_of(queue);
    return ::av::vkproto::rvk_vkQueueBindSparse(conn->transport(), queue, bindInfoCount, pBindInfo, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("vkQueueSubmit2");
    auto* conn = ::av::icd::connection_of(queue);
    return ::av::vkproto::rvk_vkQueueSubmit2(conn->transport(), queue, submitCount, pSubmits, fence);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("vkResetQueryPool");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkResetQueryPool(conn->transport(), device, queryPool, firstQuery, queryCount);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    AV_SCOPE("vkSetPrivateData");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkSetPrivateData(conn->transport(), device, objectType, objectHandle, privateDataSlot, data);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("vkSignalSemaphore");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkSignalSemaphore(conn->transport(), device, pSignalInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("vkTrimCommandPool");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkTrimCommandPool(conn->transport(), device, commandPool, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("vkUpdateDescriptorSetWithTemplate");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkUpdateDescriptorSetWithTemplate(conn->transport(), device, descriptorSet, descriptorUpdateTemplate, pData);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("vkWaitSemaphores");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkWaitSemaphores(conn->transport(), device, pWaitInfo, timeout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("vkGetPhysicalDeviceFeatures2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFeatures2KHR(conn->transport(), physicalDevice, pFeatures);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceFormatProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceFormatProperties2KHR(conn->transport(), physicalDevice, format, pFormatProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("vkGetPhysicalDeviceImageFormatProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceImageFormatProperties2KHR(conn->transport(), physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("vkGetPhysicalDeviceMemoryProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceMemoryProperties2KHR(conn->transport(), physicalDevice, pMemoryProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceProperties2KHR(conn->transport(), physicalDevice, pProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties2KHR(conn->transport(), physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(conn->transport(), physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("vkBindBufferMemory2KHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindBufferMemory2KHR(conn->transport(), device, bindInfoCount, pBindInfos);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("vkBindImageMemory2KHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBindImageMemory2KHR(conn->transport(), device, bindInfoCount, pBindInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetBufferMemoryRequirements2KHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetBufferMemoryRequirements2KHR(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetImageMemoryRequirements2KHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageMemoryRequirements2KHR(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("vkGetImageSparseMemoryRequirements2KHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSparseMemoryRequirements2KHR(conn->transport(), device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("vkCmdBeginRenderPass2KHR");
    ::av::vkproto::rvk_vkCmdBeginRenderPass2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("vkCmdBeginRenderingKHR");
    ::av::vkproto::rvk_vkCmdBeginRenderingKHR(::av::icd::record_target(commandBuffer), commandBuffer, pRenderingInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("vkCmdBlitImage2KHR");
    ::av::vkproto::rvk_vkCmdBlitImage2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pBlitImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("vkCmdCopyBuffer2KHR");
    ::av::vkproto::rvk_vkCmdCopyBuffer2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pCopyBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("vkCmdCopyBufferToImage2KHR");
    ::av::vkproto::rvk_vkCmdCopyBufferToImage2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pCopyBufferToImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("vkCmdCopyImage2KHR");
    ::av::vkproto::rvk_vkCmdCopyImage2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pCopyImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("vkCmdCopyImageToBuffer2KHR");
    ::av::vkproto::rvk_vkCmdCopyImageToBuffer2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pCopyImageToBufferInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("vkCmdDispatchBaseKHR");
    ::av::vkproto::rvk_vkCmdDispatchBaseKHR(::av::icd::record_target(commandBuffer), commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndexedIndirectCountKHR");
    ::av::vkproto::rvk_vkCmdDrawIndexedIndirectCountKHR(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawIndirectCountKHR");
    ::av::vkproto::rvk_vkCmdDrawIndirectCountKHR(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("vkCmdEndRenderPass2KHR");
    ::av::vkproto::rvk_vkCmdEndRenderPass2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) {
    AV_SCOPE("vkCmdEndRenderingKHR");
    ::av::vkproto::rvk_vkCmdEndRenderingKHR(::av::icd::record_target(commandBuffer), commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("vkCmdNextSubpass2KHR");
    ::av::vkproto::rvk_vkCmdNextSubpass2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("vkCmdPipelineBarrier2KHR");
    ::av::vkproto::rvk_vkCmdPipelineBarrier2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
    AV_SCOPE("vkCmdPushDescriptorSetWithTemplateKHR");
    ::av::vkproto::rvk_vkCmdPushDescriptorSetWithTemplateKHR(::av::icd::record_target(commandBuffer), commandBuffer, descriptorUpdateTemplate, layout, set, pData);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("vkCmdResetEvent2KHR");
    ::av::vkproto::rvk_vkCmdResetEvent2KHR(::av::icd::record_target(commandBuffer), commandBuffer, event, stageMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("vkCmdResolveImage2KHR");
    ::av::vkproto::rvk_vkCmdResolveImage2KHR(::av::icd::record_target(commandBuffer), commandBuffer, pResolveImageInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("vkCmdSetDeviceMaskKHR");
    ::av::vkproto::rvk_vkCmdSetDeviceMaskKHR(::av::icd::record_target(commandBuffer), commandBuffer, deviceMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("vkCmdSetEvent2KHR");
    ::av::vkproto::rvk_vkCmdSetEvent2KHR(::av::icd::record_target(commandBuffer), commandBuffer, event, pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("vkCmdWaitEvents2KHR");
    ::av::vkproto::rvk_vkCmdWaitEvents2KHR(::av::icd::record_target(commandBuffer), commandBuffer, eventCount, pEvents, pDependencyInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("vkCmdWriteTimestamp2KHR");
    ::av::vkproto::rvk_vkCmdWriteTimestamp2KHR(::av::icd::record_target(commandBuffer), commandBuffer, stage, queryPool, query);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("vkCreateDescriptorUpdateTemplateKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateDescriptorUpdateTemplateKHR(conn->transport(), device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("vkCreateRenderPass2KHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateRenderPass2KHR(conn->transport(), device, pCreateInfo, pAllocator, pRenderPass);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("vkCreateSamplerYcbcrConversionKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateSamplerYcbcrConversionKHR(conn->transport(), device, pCreateInfo, pAllocator, pYcbcrConversion);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDescriptorUpdateTemplateKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDescriptorUpdateTemplateKHR(conn->transport(), device, descriptorUpdateTemplate, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroySamplerYcbcrConversionKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroySamplerYcbcrConversionKHR(conn->transport(), device, ycbcrConversion, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("vkEnumeratePhysicalDeviceGroupsKHR");
    auto* conn = ::av::icd::connection_of(instance);
    if (pPhysicalDeviceGroupProperties == nullptr) {
        return ::av::vkproto::rvk_vkEnumeratePhysicalDeviceGroupsKHR(conn->transport(), instance, pPhysicalDeviceGroupCount, nullptr);
    }
    const VkResult rvk_rc = ::av::vkproto::rvk_vkEnumeratePhysicalDeviceGroupsKHR(conn->transport(), instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    if (rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE) {
        return rvk_rc;
    }
    for (uint32_t k = 0; k < *pPhysicalDeviceGroupCount; ++k) {
        for (uint32_t j = 0; j < pPhysicalDeviceGroupProperties[k].physicalDeviceCount; ++j) {
            pPhysicalDeviceGroupProperties[k].physicalDevices[j] = ::av::icd::wrap_server_handle<VkPhysicalDevice>(reinterpret_cast<uint64_t>(pPhysicalDeviceGroupProperties[k].physicalDevices[j]), conn);
            if (pPhysicalDeviceGroupProperties[k].physicalDevices[j] == VK_NULL_HANDLE) {
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }
    return rvk_rc;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL Wrap_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("vkGetBufferDeviceAddressKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetBufferDeviceAddressKHR(conn->transport(), device, pInfo);
}

VKAPI_ATTR uint64_t VKAPI_CALL Wrap_vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("vkGetBufferOpaqueCaptureAddressKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetBufferOpaqueCaptureAddressKHR(conn->transport(), device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("vkGetDescriptorSetLayoutSupportKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDescriptorSetLayoutSupportKHR(conn->transport(), device, pCreateInfo, pSupport);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetDeviceBufferMemoryRequirementsKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceBufferMemoryRequirementsKHR(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("vkGetDeviceGroupPeerMemoryFeaturesKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceGroupPeerMemoryFeaturesKHR(conn->transport(), device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("vkGetDeviceImageMemoryRequirementsKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceImageMemoryRequirementsKHR(conn->transport(), device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("vkGetDeviceImageSparseMemoryRequirementsKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceImageSparseMemoryRequirementsKHR(conn->transport(), device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VKAPI_ATTR uint64_t VKAPI_CALL Wrap_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("vkGetDeviceMemoryOpaqueCaptureAddressKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeviceMemoryOpaqueCaptureAddressKHR(conn->transport(), device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalBufferPropertiesKHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalBufferPropertiesKHR(conn->transport(), physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalFencePropertiesKHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalFencePropertiesKHR(conn->transport(), physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    ::av::vkproto::rvk_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(conn->transport(), physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("vkGetSemaphoreCounterValueKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetSemaphoreCounterValueKHR(conn->transport(), device, semaphore, pValue);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("vkQueueSubmit2KHR");
    auto* conn = ::av::icd::connection_of(queue);
    return ::av::vkproto::rvk_vkQueueSubmit2KHR(conn->transport(), queue, submitCount, pSubmits, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("vkSignalSemaphoreKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkSignalSemaphoreKHR(conn->transport(), device, pSignalInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("vkTrimCommandPoolKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkTrimCommandPoolKHR(conn->transport(), device, commandPool, flags);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("vkUpdateDescriptorSetWithTemplateKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkUpdateDescriptorSetWithTemplateKHR(conn->transport(), device, descriptorSet, descriptorUpdateTemplate, pData);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("vkWaitSemaphoresKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkWaitSemaphoresKHR(conn->transport(), device, pWaitInfo, timeout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    AV_SCOPE("vkCmdBeginConditionalRenderingEXT");
    ::av::vkproto::rvk_vkCmdBeginConditionalRenderingEXT(::av::icd::record_target(commandBuffer), commandBuffer, pConditionalRenderingBegin);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) {
    AV_SCOPE("vkCmdEndConditionalRenderingEXT");
    ::av::vkproto::rvk_vkCmdEndConditionalRenderingEXT(::av::icd::record_target(commandBuffer), commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    AV_SCOPE("vkCmdSetColorWriteEnableEXT");
    ::av::vkproto::rvk_vkCmdSetColorWriteEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, attachmentCount, pColorWriteEnables);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("vkCmdSetDepthBiasEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthBiasEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthBiasEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    AV_SCOPE("vkCmdSetLogicOpEXT");
    ::av::vkproto::rvk_vkCmdSetLogicOpEXT(::av::icd::record_target(commandBuffer), commandBuffer, logicOp);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    AV_SCOPE("vkCmdSetPatchControlPointsEXT");
    ::av::vkproto::rvk_vkCmdSetPatchControlPointsEXT(::av::icd::record_target(commandBuffer), commandBuffer, patchControlPoints);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("vkCmdSetPrimitiveRestartEnableEXT");
    ::av::vkproto::rvk_vkCmdSetPrimitiveRestartEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, primitiveRestartEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("vkCmdSetRasterizerDiscardEnableEXT");
    ::av::vkproto::rvk_vkCmdSetRasterizerDiscardEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, rasterizerDiscardEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    AV_SCOPE("vkCmdSetVertexInputEXT");
    ::av::vkproto::rvk_vkCmdSetVertexInputEXT(::av::icd::record_target(commandBuffer), commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("vkBuildAccelerationStructuresKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkBuildAccelerationStructuresKHR(conn->transport(), device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
    AV_SCOPE("vkCmdBindShadersEXT");
    ::av::vkproto::rvk_vkCmdBindShadersEXT(::av::icd::record_target(commandBuffer), commandBuffer, stageCount, pStages, pShaders);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("vkCmdBindVertexBuffers2EXT");
    ::av::vkproto::rvk_vkCmdBindVertexBuffers2EXT(::av::icd::record_target(commandBuffer), commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) {
    AV_SCOPE("vkCmdBuildAccelerationStructuresIndirectKHR");
    ::av::vkproto::rvk_vkCmdBuildAccelerationStructuresIndirectKHR(::av::icd::record_target(commandBuffer), commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("vkCmdBuildAccelerationStructuresKHR");
    ::av::vkproto::rvk_vkCmdBuildAccelerationStructuresKHR(::av::icd::record_target(commandBuffer), commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("vkCmdCopyAccelerationStructureKHR");
    ::av::vkproto::rvk_vkCmdCopyAccelerationStructureKHR(::av::icd::record_target(commandBuffer), commandBuffer, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("vkCmdCopyAccelerationStructureToMemoryKHR");
    ::av::vkproto::rvk_vkCmdCopyAccelerationStructureToMemoryKHR(::av::icd::record_target(commandBuffer), commandBuffer, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("vkCmdCopyMemoryToAccelerationStructureKHR");
    ::av::vkproto::rvk_vkCmdCopyMemoryToAccelerationStructureKHR(::av::icd::record_target(commandBuffer), commandBuffer, pInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("vkCmdDrawMeshTasksEXT");
    ::av::vkproto::rvk_vkCmdDrawMeshTasksEXT(::av::icd::record_target(commandBuffer), commandBuffer, groupCountX, groupCountY, groupCountZ);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawMeshTasksIndirectCountEXT");
    ::av::vkproto::rvk_vkCmdDrawMeshTasksIndirectCountEXT(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("vkCmdDrawMeshTasksIndirectEXT");
    ::av::vkproto::rvk_vkCmdDrawMeshTasksIndirectEXT(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, drawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    AV_SCOPE("vkCmdPushDescriptorSetKHR");
    ::av::vkproto::rvk_vkCmdPushDescriptorSetKHR(::av::icd::record_target(commandBuffer), commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    AV_SCOPE("vkCmdSetAlphaToCoverageEnableEXT");
    ::av::vkproto::rvk_vkCmdSetAlphaToCoverageEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, alphaToCoverageEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    AV_SCOPE("vkCmdSetAlphaToOneEnableEXT");
    ::av::vkproto::rvk_vkCmdSetAlphaToOneEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, alphaToOneEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    AV_SCOPE("vkCmdSetColorBlendAdvancedEXT");
    ::av::vkproto::rvk_vkCmdSetColorBlendAdvancedEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    AV_SCOPE("vkCmdSetColorBlendEnableEXT");
    ::av::vkproto::rvk_vkCmdSetColorBlendEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    AV_SCOPE("vkCmdSetColorBlendEquationEXT");
    ::av::vkproto::rvk_vkCmdSetColorBlendEquationEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    AV_SCOPE("vkCmdSetColorWriteMaskEXT");
    ::av::vkproto::rvk_vkCmdSetColorWriteMaskEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    AV_SCOPE("vkCmdSetConservativeRasterizationModeEXT");
    ::av::vkproto::rvk_vkCmdSetConservativeRasterizationModeEXT(::av::icd::record_target(commandBuffer), commandBuffer, conservativeRasterizationMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    AV_SCOPE("vkCmdSetCoverageModulationModeNV");
    ::av::vkproto::rvk_vkCmdSetCoverageModulationModeNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageModulationMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    AV_SCOPE("vkCmdSetCoverageModulationTableEnableNV");
    ::av::vkproto::rvk_vkCmdSetCoverageModulationTableEnableNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageModulationTableEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    AV_SCOPE("vkCmdSetCoverageModulationTableNV");
    ::av::vkproto::rvk_vkCmdSetCoverageModulationTableNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageModulationTableCount, pCoverageModulationTable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    AV_SCOPE("vkCmdSetCoverageReductionModeNV");
    ::av::vkproto::rvk_vkCmdSetCoverageReductionModeNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageReductionMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    AV_SCOPE("vkCmdSetCoverageToColorEnableNV");
    ::av::vkproto::rvk_vkCmdSetCoverageToColorEnableNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageToColorEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    AV_SCOPE("vkCmdSetCoverageToColorLocationNV");
    ::av::vkproto::rvk_vkCmdSetCoverageToColorLocationNV(::av::icd::record_target(commandBuffer), commandBuffer, coverageToColorLocation);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("vkCmdSetCullModeEXT");
    ::av::vkproto::rvk_vkCmdSetCullModeEXT(::av::icd::record_target(commandBuffer), commandBuffer, cullMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("vkCmdSetDepthBoundsTestEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthBoundsTestEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthBoundsTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    AV_SCOPE("vkCmdSetDepthClampEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthClampEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthClampEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthClampRangeEXT(VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
    AV_SCOPE("vkCmdSetDepthClampRangeEXT");
    ::av::vkproto::rvk_vkCmdSetDepthClampRangeEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthClampMode, pDepthClampRange);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    AV_SCOPE("vkCmdSetDepthClipEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthClipEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthClipEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    AV_SCOPE("vkCmdSetDepthClipNegativeOneToOneEXT");
    ::av::vkproto::rvk_vkCmdSetDepthClipNegativeOneToOneEXT(::av::icd::record_target(commandBuffer), commandBuffer, negativeOneToOne);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("vkCmdSetDepthCompareOpEXT");
    ::av::vkproto::rvk_vkCmdSetDepthCompareOpEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthCompareOp);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("vkCmdSetDepthTestEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthTestEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("vkCmdSetDepthWriteEnableEXT");
    ::av::vkproto::rvk_vkCmdSetDepthWriteEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, depthWriteEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    AV_SCOPE("vkCmdSetExtraPrimitiveOverestimationSizeEXT");
    ::av::vkproto::rvk_vkCmdSetExtraPrimitiveOverestimationSizeEXT(::av::icd::record_target(commandBuffer), commandBuffer, extraPrimitiveOverestimationSize);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) {
    AV_SCOPE("vkCmdSetFragmentShadingRateKHR");
    ::av::vkproto::rvk_vkCmdSetFragmentShadingRateKHR(::av::icd::record_target(commandBuffer), commandBuffer, pFragmentSize, combinerOps);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("vkCmdSetFrontFaceEXT");
    ::av::vkproto::rvk_vkCmdSetFrontFaceEXT(::av::icd::record_target(commandBuffer), commandBuffer, frontFace);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    AV_SCOPE("vkCmdSetLineRasterizationModeEXT");
    ::av::vkproto::rvk_vkCmdSetLineRasterizationModeEXT(::av::icd::record_target(commandBuffer), commandBuffer, lineRasterizationMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    AV_SCOPE("vkCmdSetLineStippleEXT");
    ::av::vkproto::rvk_vkCmdSetLineStippleEXT(::av::icd::record_target(commandBuffer), commandBuffer, lineStippleFactor, lineStipplePattern);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    AV_SCOPE("vkCmdSetLineStippleEnableEXT");
    ::av::vkproto::rvk_vkCmdSetLineStippleEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, stippledLineEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    AV_SCOPE("vkCmdSetLogicOpEnableEXT");
    ::av::vkproto::rvk_vkCmdSetLogicOpEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, logicOpEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    AV_SCOPE("vkCmdSetPolygonModeEXT");
    ::av::vkproto::rvk_vkCmdSetPolygonModeEXT(::av::icd::record_target(commandBuffer), commandBuffer, polygonMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("vkCmdSetPrimitiveTopologyEXT");
    ::av::vkproto::rvk_vkCmdSetPrimitiveTopologyEXT(::av::icd::record_target(commandBuffer), commandBuffer, primitiveTopology);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    AV_SCOPE("vkCmdSetProvokingVertexModeEXT");
    ::av::vkproto::rvk_vkCmdSetProvokingVertexModeEXT(::av::icd::record_target(commandBuffer), commandBuffer, provokingVertexMode);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    AV_SCOPE("vkCmdSetRasterizationSamplesEXT");
    ::av::vkproto::rvk_vkCmdSetRasterizationSamplesEXT(::av::icd::record_target(commandBuffer), commandBuffer, rasterizationSamples);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    AV_SCOPE("vkCmdSetRasterizationStreamEXT");
    ::av::vkproto::rvk_vkCmdSetRasterizationStreamEXT(::av::icd::record_target(commandBuffer), commandBuffer, rasterizationStream);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
    AV_SCOPE("vkCmdSetRayTracingPipelineStackSizeKHR");
    ::av::vkproto::rvk_vkCmdSetRayTracingPipelineStackSizeKHR(::av::icd::record_target(commandBuffer), commandBuffer, pipelineStackSize);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    AV_SCOPE("vkCmdSetRepresentativeFragmentTestEnableNV");
    ::av::vkproto::rvk_vkCmdSetRepresentativeFragmentTestEnableNV(::av::icd::record_target(commandBuffer), commandBuffer, representativeFragmentTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    AV_SCOPE("vkCmdSetSampleLocationsEnableEXT");
    ::av::vkproto::rvk_vkCmdSetSampleLocationsEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, sampleLocationsEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    AV_SCOPE("vkCmdSetSampleMaskEXT");
    ::av::vkproto::rvk_vkCmdSetSampleMaskEXT(::av::icd::record_target(commandBuffer), commandBuffer, samples, pSampleMask);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("vkCmdSetScissorWithCountEXT");
    ::av::vkproto::rvk_vkCmdSetScissorWithCountEXT(::av::icd::record_target(commandBuffer), commandBuffer, scissorCount, pScissors);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    AV_SCOPE("vkCmdSetShadingRateImageEnableNV");
    ::av::vkproto::rvk_vkCmdSetShadingRateImageEnableNV(::av::icd::record_target(commandBuffer), commandBuffer, shadingRateImageEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("vkCmdSetStencilOpEXT");
    ::av::vkproto::rvk_vkCmdSetStencilOpEXT(::av::icd::record_target(commandBuffer), commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("vkCmdSetStencilTestEnableEXT");
    ::av::vkproto::rvk_vkCmdSetStencilTestEnableEXT(::av::icd::record_target(commandBuffer), commandBuffer, stencilTestEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    AV_SCOPE("vkCmdSetTessellationDomainOriginEXT");
    ::av::vkproto::rvk_vkCmdSetTessellationDomainOriginEXT(::av::icd::record_target(commandBuffer), commandBuffer, domainOrigin);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    AV_SCOPE("vkCmdSetViewportSwizzleNV");
    ::av::vkproto::rvk_vkCmdSetViewportSwizzleNV(::av::icd::record_target(commandBuffer), commandBuffer, firstViewport, viewportCount, pViewportSwizzles);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    AV_SCOPE("vkCmdSetViewportWScalingEnableNV");
    ::av::vkproto::rvk_vkCmdSetViewportWScalingEnableNV(::av::icd::record_target(commandBuffer), commandBuffer, viewportWScalingEnable);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("vkCmdSetViewportWithCountEXT");
    ::av::vkproto::rvk_vkCmdSetViewportWithCountEXT(::av::icd::record_target(commandBuffer), commandBuffer, viewportCount, pViewports);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
    AV_SCOPE("vkCmdTraceRaysIndirectKHR");
    ::av::vkproto::rvk_vkCmdTraceRaysIndirectKHR(::av::icd::record_target(commandBuffer), commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
    AV_SCOPE("vkCmdTraceRaysKHR");
    ::av::vkproto::rvk_vkCmdTraceRaysKHR(::av::icd::record_target(commandBuffer), commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
    AV_SCOPE("vkCmdWriteAccelerationStructuresPropertiesKHR");
    ::av::vkproto::rvk_vkCmdWriteAccelerationStructuresPropertiesKHR(::av::icd::record_target(commandBuffer), commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("vkCopyAccelerationStructureKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyAccelerationStructureKHR(conn->transport(), device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("vkCopyAccelerationStructureToMemoryKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyAccelerationStructureToMemoryKHR(conn->transport(), device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyImageToImageEXT(VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
    AV_SCOPE("vkCopyImageToImageEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyImageToImageEXT(conn->transport(), device, pCopyImageToImageInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyImageToMemoryEXT(VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
    AV_SCOPE("vkCopyImageToMemoryEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyImageToMemoryEXT(conn->transport(), device, pCopyImageToMemoryInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("vkCopyMemoryToAccelerationStructureKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyMemoryToAccelerationStructureKHR(conn->transport(), device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCopyMemoryToImageEXT(VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
    AV_SCOPE("vkCopyMemoryToImageEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCopyMemoryToImageEXT(conn->transport(), device, pCopyMemoryToImageInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) {
    AV_SCOPE("vkCreateAccelerationStructureKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateAccelerationStructureKHR(conn->transport(), device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) {
    AV_SCOPE("vkCreateDeferredOperationKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateDeferredOperationKHR(conn->transport(), device, pAllocator, pDeferredOperation);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("vkCreateRayTracingPipelinesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateRayTracingPipelinesKHR(conn->transport(), device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) {
    AV_SCOPE("vkCreateShadersEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreateShadersEXT(conn->transport(), device, createInfoCount, pCreateInfos, pAllocator, pShaders);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("vkDeferredOperationJoinKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkDeferredOperationJoinKHR(conn->transport(), device, operation);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyAccelerationStructureKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyAccelerationStructureKHR(conn->transport(), device, accelerationStructure, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyDeferredOperationKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyDeferredOperationKHR(conn->transport(), device, operation, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyShaderEXT");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyShaderEXT(conn->transport(), device, shader, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
    AV_SCOPE("vkGetAccelerationStructureBuildSizesKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetAccelerationStructureBuildSizesKHR(conn->transport(), device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL Wrap_vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
    AV_SCOPE("vkGetAccelerationStructureDeviceAddressKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetAccelerationStructureDeviceAddressKHR(conn->transport(), device, pInfo);
}

VKAPI_ATTR uint32_t VKAPI_CALL Wrap_vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("vkGetDeferredOperationMaxConcurrencyKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeferredOperationMaxConcurrencyKHR(conn->transport(), device, operation);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("vkGetDeferredOperationResultKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetDeferredOperationResultKHR(conn->transport(), device, operation);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
    AV_SCOPE("vkGetDeviceAccelerationStructureCompatibilityKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceAccelerationStructureCompatibilityKHR(conn->transport(), device, pVersionInfo, pCompatibility);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("vkGetImageSubresourceLayout2EXT");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSubresourceLayout2EXT(conn->transport(), device, image, pSubresource, pLayout);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
    AV_SCOPE("vkGetPhysicalDeviceFragmentShadingRatesKHR");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceFragmentShadingRatesKHR(conn->transport(), physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(conn->transport(), device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("vkGetRayTracingShaderGroupHandlesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetRayTracingShaderGroupHandlesKHR(conn->transport(), device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VKAPI_ATTR VkDeviceSize VKAPI_CALL Wrap_vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
    AV_SCOPE("vkGetRayTracingShaderGroupStackSizeKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetRayTracingShaderGroupStackSizeKHR(conn->transport(), device, pipeline, group, groupShader);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkTransitionImageLayoutEXT(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
    AV_SCOPE("vkTransitionImageLayoutEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkTransitionImageLayoutEXT(conn->transport(), device, transitionCount, pTransitions);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
    AV_SCOPE("vkWriteAccelerationStructuresPropertiesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkWriteAccelerationStructuresPropertiesKHR(conn->transport(), device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    AV_SCOPE("vkGetCalibratedTimestampsEXT");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetCalibratedTimestampsEXT(conn->transport(), device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    AV_SCOPE("vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
    auto* conn = ::av::icd::connection_of(physicalDevice);
    return ::av::vkproto::rvk_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(conn->transport(), physicalDevice, pTimeDomainCount, pTimeDomains);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
    AV_SCOPE("vkCmdSetRenderingAttachmentLocationsKHR");
    ::av::vkproto::rvk_vkCmdSetRenderingAttachmentLocationsKHR(::av::icd::record_target(commandBuffer), commandBuffer, pLocationInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
    AV_SCOPE("vkCmdSetRenderingInputAttachmentIndicesKHR");
    ::av::vkproto::rvk_vkCmdSetRenderingInputAttachmentIndicesKHR(::av::icd::record_target(commandBuffer), commandBuffer, pInputAttachmentIndexInfo);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
    AV_SCOPE("vkCmdBindIndexBuffer2KHR");
    ::av::vkproto::rvk_vkCmdBindIndexBuffer2KHR(::av::icd::record_target(commandBuffer), commandBuffer, buffer, offset, size, indexType);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
    AV_SCOPE("vkCreatePipelineBinariesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkCreatePipelineBinariesKHR(conn->transport(), device, pCreateInfo, pAllocator, pBinaries);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyPipelineBinaryKHR(VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkDestroyPipelineBinaryKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkDestroyPipelineBinaryKHR(conn->transport(), device, pipelineBinary, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("vkGetDeviceImageSubresourceLayoutKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetDeviceImageSubresourceLayoutKHR(conn->transport(), device, pInfo, pLayout);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetImageSubresourceLayout2KHR(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("vkGetImageSubresourceLayout2KHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetImageSubresourceLayout2KHR(conn->transport(), device, image, pSubresource, pLayout);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
    AV_SCOPE("vkGetPipelineKeyKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetPipelineKeyKHR(conn->transport(), device, pPipelineCreateInfo, pPipelineKey);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkGetRenderingAreaGranularityKHR(VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
    AV_SCOPE("vkGetRenderingAreaGranularityKHR");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkGetRenderingAreaGranularityKHR(conn->transport(), device, pRenderingAreaInfo, pGranularity);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkReleaseCapturedPipelineDataKHR(VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("vkReleaseCapturedPipelineDataKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkReleaseCapturedPipelineDataKHR(conn->transport(), device, pInfo, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    AV_SCOPE("vkCmdBeginQueryIndexedEXT");
    ::av::vkproto::rvk_vkCmdBeginQueryIndexedEXT(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, query, flags, index);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("vkCmdBeginTransformFeedbackEXT");
    ::av::vkproto::rvk_vkCmdBeginTransformFeedbackEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    AV_SCOPE("vkCmdBindTransformFeedbackBuffersEXT");
    ::av::vkproto::rvk_vkCmdBindTransformFeedbackBuffersEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    AV_SCOPE("vkCmdDrawIndirectByteCountEXT");
    ::av::vkproto::rvk_vkCmdDrawIndirectByteCountEXT(::av::icd::record_target(commandBuffer), commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    AV_SCOPE("vkCmdEndQueryIndexedEXT");
    ::av::vkproto::rvk_vkCmdEndQueryIndexedEXT(::av::icd::record_target(commandBuffer), commandBuffer, queryPool, query, index);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("vkCmdEndTransformFeedbackEXT");
    ::av::vkproto::rvk_vkCmdEndTransformFeedbackEXT(::av::icd::record_target(commandBuffer), commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority) {
    AV_SCOPE("vkSetDeviceMemoryPriorityEXT");
    auto* conn = ::av::icd::connection_of(device);
    ::av::vkproto::rvk_vkSetDeviceMemoryPriorityEXT(conn->transport(), device, memory, priority);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("vkGetFenceFdKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetFenceFdKHR(conn->transport(), device, pGetFdInfo, pFd);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("vkGetMemoryFdKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetMemoryFdKHR(conn->transport(), device, pGetFdInfo, pFd);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    AV_SCOPE("vkGetMemoryFdPropertiesKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetMemoryFdPropertiesKHR(conn->transport(), device, handleType, fd, pMemoryFdProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("vkGetSemaphoreFdKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkGetSemaphoreFdKHR(conn->transport(), device, pGetFdInfo, pFd);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    AV_SCOPE("vkImportFenceFdKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkImportFenceFdKHR(conn->transport(), device, pImportFenceFdInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    AV_SCOPE("vkImportSemaphoreFdKHR");
    auto* conn = ::av::icd::connection_of(device);
    return ::av::vkproto::rvk_vkImportSemaphoreFdKHR(conn->transport(), device, pImportSemaphoreFdInfo);
}

namespace {
const Entry kWrapperTable[388] = {
    Entry{ "vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyInstance), DispatchLevel::instance },
    Entry{ "vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDevice), DispatchLevel::device },
    Entry{ "vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceQueue), DispatchLevel::device },
    Entry{ "vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEnumeratePhysicalDevices), DispatchLevel::instance },
    Entry{ "vkGetPhysicalDeviceProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceProperties2), DispatchLevel::physical_device },
    Entry{ "vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateInstance), DispatchLevel::global },
    Entry{ "vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDevice), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFeatures), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFormatProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceImageFormatProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceMemoryProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceQueueFamilyProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceSparseImageFormatProperties), DispatchLevel::physical_device },
    Entry{ "vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEnumerateDeviceExtensionProperties), DispatchLevel::physical_device },
    Entry{ "vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEnumerateDeviceLayerProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceFeatures2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFeatures2), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceFormatProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFormatProperties2), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceImageFormatProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceImageFormatProperties2), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceMemoryProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceMemoryProperties2), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceQueueFamilyProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceQueueFamilyProperties2), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceSparseImageFormatProperties2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceSparseImageFormatProperties2), DispatchLevel::physical_device },
    Entry{ "vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDeviceWaitIdle), DispatchLevel::device },
    Entry{ "vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkQueueWaitIdle), DispatchLevel::device },
    Entry{ "vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkAllocateMemory), DispatchLevel::device },
    Entry{ "vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkFreeMemory), DispatchLevel::device },
    Entry{ "vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindBufferMemory), DispatchLevel::device },
    Entry{ "vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindImageMemory), DispatchLevel::device },
    Entry{ "vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferMemoryRequirements), DispatchLevel::device },
    Entry{ "vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageMemoryRequirements), DispatchLevel::device },
    Entry{ "vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateBuffer), DispatchLevel::device },
    Entry{ "vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyBuffer), DispatchLevel::device },
    Entry{ "vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateImage), DispatchLevel::device },
    Entry{ "vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyImage), DispatchLevel::device },
    Entry{ "vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateImageView), DispatchLevel::device },
    Entry{ "vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyImageView), DispatchLevel::device },
    Entry{ "vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateSampler), DispatchLevel::device },
    Entry{ "vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroySampler), DispatchLevel::device },
    Entry{ "vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateSemaphore), DispatchLevel::device },
    Entry{ "vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroySemaphore), DispatchLevel::device },
    Entry{ "vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateFence), DispatchLevel::device },
    Entry{ "vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyFence), DispatchLevel::device },
    Entry{ "vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetFences), DispatchLevel::device },
    Entry{ "vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkWaitForFences), DispatchLevel::device },
    Entry{ "vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetFenceStatus), DispatchLevel::device },
    Entry{ "vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateCommandPool), DispatchLevel::device },
    Entry{ "vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyCommandPool), DispatchLevel::device },
    Entry{ "vkResetCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetCommandPool), DispatchLevel::device },
    Entry{ "vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBeginCommandBuffer), DispatchLevel::device },
    Entry{ "vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEndCommandBuffer), DispatchLevel::device },
    Entry{ "vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetCommandBuffer), DispatchLevel::device },
    Entry{ "vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyShaderModule), DispatchLevel::device },
    Entry{ "vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateShaderModule), DispatchLevel::device },
    Entry{ "vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreatePipelineLayout), DispatchLevel::device },
    Entry{ "vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyPipelineLayout), DispatchLevel::device },
    Entry{ "vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyPipeline), DispatchLevel::device },
    Entry{ "vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreatePipelineCache), DispatchLevel::device },
    Entry{ "vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyPipelineCache), DispatchLevel::device },
    Entry{ "vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateRenderPass), DispatchLevel::device },
    Entry{ "vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyRenderPass), DispatchLevel::device },
    Entry{ "vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateFramebuffer), DispatchLevel::device },
    Entry{ "vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyFramebuffer), DispatchLevel::device },
    Entry{ "vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDescriptorSetLayout), DispatchLevel::device },
    Entry{ "vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDescriptorSetLayout), DispatchLevel::device },
    Entry{ "vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDescriptorPool), DispatchLevel::device },
    Entry{ "vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDescriptorPool), DispatchLevel::device },
    Entry{ "vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkFreeCommandBuffers), DispatchLevel::device },
    Entry{ "vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkQueueSubmit), DispatchLevel::device },
    Entry{ "vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindPipeline), DispatchLevel::device },
    Entry{ "vkCmdSetViewport", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetViewport), DispatchLevel::device },
    Entry{ "vkCmdSetScissor", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetScissor), DispatchLevel::device },
    Entry{ "vkCmdBindVertexBuffers", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindVertexBuffers), DispatchLevel::device },
    Entry{ "vkCmdBindIndexBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindIndexBuffer), DispatchLevel::device },
    Entry{ "vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindDescriptorSets), DispatchLevel::device },
    Entry{ "vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDraw), DispatchLevel::device },
    Entry{ "vkCmdDrawIndexed", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndexed), DispatchLevel::device },
    Entry{ "vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPipelineBarrier), DispatchLevel::device },
    Entry{ "vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBuffer), DispatchLevel::device },
    Entry{ "vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginRenderPass), DispatchLevel::device },
    Entry{ "vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndRenderPass), DispatchLevel::device },
    Entry{ "vkCmdNextSubpass", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdNextSubpass), DispatchLevel::device },
    Entry{ "vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPushConstants), DispatchLevel::device },
    Entry{ "vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateGraphicsPipelines), DispatchLevel::device },
    Entry{ "vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkUpdateDescriptorSets), DispatchLevel::device },
    Entry{ "vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkAllocateCommandBuffers), DispatchLevel::device },
    Entry{ "vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkAllocateDescriptorSets), DispatchLevel::device },
    Entry{ "vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkFreeDescriptorSets), DispatchLevel::device },
    Entry{ "vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateComputePipelines), DispatchLevel::device },
    Entry{ "vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetDescriptorPool), DispatchLevel::device },
    Entry{ "vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateEvent), DispatchLevel::device },
    Entry{ "vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyEvent), DispatchLevel::device },
    Entry{ "vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetEventStatus), DispatchLevel::device },
    Entry{ "vkSetEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkSetEvent), DispatchLevel::device },
    Entry{ "vkResetEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetEvent), DispatchLevel::device },
    Entry{ "vkCmdSetEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetEvent), DispatchLevel::device },
    Entry{ "vkCmdResetEvent", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResetEvent), DispatchLevel::device },
    Entry{ "vkCmdWaitEvents", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWaitEvents), DispatchLevel::device },
    Entry{ "vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImage), DispatchLevel::device },
    Entry{ "vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBufferToImage), DispatchLevel::device },
    Entry{ "vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImageToBuffer), DispatchLevel::device },
    Entry{ "vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBlitImage), DispatchLevel::device },
    Entry{ "vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdClearColorImage), DispatchLevel::device },
    Entry{ "vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdClearDepthStencilImage), DispatchLevel::device },
    Entry{ "vkCmdClearAttachments", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdClearAttachments), DispatchLevel::device },
    Entry{ "vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdFillBuffer), DispatchLevel::device },
    Entry{ "vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdUpdateBuffer), DispatchLevel::device },
    Entry{ "vkCmdSetLineWidth", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLineWidth), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBias", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBias), DispatchLevel::device },
    Entry{ "vkCmdSetBlendConstants", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetBlendConstants), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBounds", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBounds), DispatchLevel::device },
    Entry{ "vkCmdSetStencilCompareMask", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilCompareMask), DispatchLevel::device },
    Entry{ "vkCmdSetStencilWriteMask", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilWriteMask), DispatchLevel::device },
    Entry{ "vkCmdSetStencilReference", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilReference), DispatchLevel::device },
    Entry{ "vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDispatch), DispatchLevel::device },
    Entry{ "vkCmdDispatchIndirect", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDispatchIndirect), DispatchLevel::device },
    Entry{ "vkCmdDrawIndirect", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndirect), DispatchLevel::device },
    Entry{ "vkCmdDrawIndexedIndirect", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndexedIndirect), DispatchLevel::device },
    Entry{ "vkCmdExecuteCommands", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdExecuteCommands), DispatchLevel::device },
    Entry{ "vkAcquireNextImage2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkAcquireNextImage2KHR), DispatchLevel::device },
    Entry{ "vkBindBufferMemory2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindBufferMemory2), DispatchLevel::device },
    Entry{ "vkBindImageMemory2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindImageMemory2), DispatchLevel::device },
    Entry{ "vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginQuery), DispatchLevel::device },
    Entry{ "vkCmdBeginRenderPass2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginRenderPass2), DispatchLevel::device },
    Entry{ "vkCmdBeginRendering", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginRendering), DispatchLevel::device },
    Entry{ "vkCmdBindVertexBuffers2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindVertexBuffers2), DispatchLevel::device },
    Entry{ "vkCmdBlitImage2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBlitImage2), DispatchLevel::device },
    Entry{ "vkCmdCopyBuffer2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBuffer2), DispatchLevel::device },
    Entry{ "vkCmdCopyBufferToImage2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBufferToImage2), DispatchLevel::device },
    Entry{ "vkCmdCopyImage2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImage2), DispatchLevel::device },
    Entry{ "vkCmdCopyImageToBuffer2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImageToBuffer2), DispatchLevel::device },
    Entry{ "vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyQueryPoolResults), DispatchLevel::device },
    Entry{ "vkCmdDispatchBase", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDispatchBase), DispatchLevel::device },
    Entry{ "vkCmdDrawIndexedIndirectCount", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndexedIndirectCount), DispatchLevel::device },
    Entry{ "vkCmdDrawIndirectCount", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndirectCount), DispatchLevel::device },
    Entry{ "vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndQuery), DispatchLevel::device },
    Entry{ "vkCmdEndRenderPass2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndRenderPass2), DispatchLevel::device },
    Entry{ "vkCmdEndRendering", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndRendering), DispatchLevel::device },
    Entry{ "vkCmdNextSubpass2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdNextSubpass2), DispatchLevel::device },
    Entry{ "vkCmdPipelineBarrier2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPipelineBarrier2), DispatchLevel::device },
    Entry{ "vkCmdResetEvent2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResetEvent2), DispatchLevel::device },
    Entry{ "vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResetQueryPool), DispatchLevel::device },
    Entry{ "vkCmdResolveImage", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResolveImage), DispatchLevel::device },
    Entry{ "vkCmdResolveImage2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResolveImage2), DispatchLevel::device },
    Entry{ "vkCmdSetCullMode", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCullMode), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBiasEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBiasEnable), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBoundsTestEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBoundsTestEnable), DispatchLevel::device },
    Entry{ "vkCmdSetDepthCompareOp", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthCompareOp), DispatchLevel::device },
    Entry{ "vkCmdSetDepthTestEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthTestEnable), DispatchLevel::device },
    Entry{ "vkCmdSetDepthWriteEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthWriteEnable), DispatchLevel::device },
    Entry{ "vkCmdSetDeviceMask", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDeviceMask), DispatchLevel::device },
    Entry{ "vkCmdSetEvent2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetEvent2), DispatchLevel::device },
    Entry{ "vkCmdSetFrontFace", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetFrontFace), DispatchLevel::device },
    Entry{ "vkCmdSetPrimitiveRestartEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPrimitiveRestartEnable), DispatchLevel::device },
    Entry{ "vkCmdSetPrimitiveTopology", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPrimitiveTopology), DispatchLevel::device },
    Entry{ "vkCmdSetRasterizerDiscardEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRasterizerDiscardEnable), DispatchLevel::device },
    Entry{ "vkCmdSetScissorWithCount", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetScissorWithCount), DispatchLevel::device },
    Entry{ "vkCmdSetStencilOp", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilOp), DispatchLevel::device },
    Entry{ "vkCmdSetStencilTestEnable", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilTestEnable), DispatchLevel::device },
    Entry{ "vkCmdSetViewportWithCount", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetViewportWithCount), DispatchLevel::device },
    Entry{ "vkCmdWaitEvents2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWaitEvents2), DispatchLevel::device },
    Entry{ "vkCmdWriteTimestamp", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWriteTimestamp), DispatchLevel::device },
    Entry{ "vkCmdWriteTimestamp2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWriteTimestamp2), DispatchLevel::device },
    Entry{ "vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateBufferView), DispatchLevel::device },
    Entry{ "vkCreateDescriptorUpdateTemplate", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDescriptorUpdateTemplate), DispatchLevel::device },
    Entry{ "vkCreatePrivateDataSlot", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreatePrivateDataSlot), DispatchLevel::device },
    Entry{ "vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateQueryPool), DispatchLevel::device },
    Entry{ "vkCreateRenderPass2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateRenderPass2), DispatchLevel::device },
    Entry{ "vkCreateSamplerYcbcrConversion", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateSamplerYcbcrConversion), DispatchLevel::device },
    Entry{ "vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyBufferView), DispatchLevel::device },
    Entry{ "vkDestroyDescriptorUpdateTemplate", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDescriptorUpdateTemplate), DispatchLevel::device },
    Entry{ "vkDestroyPrivateDataSlot", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyPrivateDataSlot), DispatchLevel::device },
    Entry{ "vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyQueryPool), DispatchLevel::device },
    Entry{ "vkDestroySamplerYcbcrConversion", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroySamplerYcbcrConversion), DispatchLevel::device },
    Entry{ "vkEnumeratePhysicalDeviceGroups", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEnumeratePhysicalDeviceGroups), DispatchLevel::instance },
    Entry{ "vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkFlushMappedMemoryRanges), DispatchLevel::device },
    Entry{ "vkGetBufferDeviceAddress", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferDeviceAddress), DispatchLevel::device },
    Entry{ "vkGetBufferMemoryRequirements2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferMemoryRequirements2), DispatchLevel::device },
    Entry{ "vkGetBufferOpaqueCaptureAddress", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferOpaqueCaptureAddress), DispatchLevel::device },
    Entry{ "vkGetDescriptorSetLayoutSupport", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDescriptorSetLayoutSupport), DispatchLevel::device },
    Entry{ "vkGetDeviceBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceBufferMemoryRequirements), DispatchLevel::device },
    Entry{ "vkGetDeviceGroupPeerMemoryFeatures", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceGroupPeerMemoryFeatures), DispatchLevel::device },
    Entry{ "vkGetDeviceGroupPresentCapabilitiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceGroupPresentCapabilitiesKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceGroupSurfacePresentModesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceGroupSurfacePresentModesKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceImageMemoryRequirements), DispatchLevel::device },
    Entry{ "vkGetDeviceImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceImageSparseMemoryRequirements), DispatchLevel::device },
    Entry{ "vkGetDeviceMemoryCommitment", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceMemoryCommitment), DispatchLevel::device },
    Entry{ "vkGetDeviceMemoryOpaqueCaptureAddress", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceMemoryOpaqueCaptureAddress), DispatchLevel::device },
    Entry{ "vkGetDeviceQueue2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceQueue2), DispatchLevel::device },
    Entry{ "vkGetImageMemoryRequirements2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageMemoryRequirements2), DispatchLevel::device },
    Entry{ "vkGetImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSparseMemoryRequirements), DispatchLevel::device },
    Entry{ "vkGetImageSparseMemoryRequirements2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSparseMemoryRequirements2), DispatchLevel::device },
    Entry{ "vkGetImageSubresourceLayout", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSubresourceLayout), DispatchLevel::device },
    Entry{ "vkGetPhysicalDeviceExternalBufferProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalBufferProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceExternalFenceProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalFenceProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceExternalSemaphoreProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalSemaphoreProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDevicePresentRectanglesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDevicePresentRectanglesKHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceToolProperties", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceToolProperties), DispatchLevel::physical_device },
    Entry{ "vkGetPrivateData", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPrivateData), DispatchLevel::device },
    Entry{ "vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetQueryPoolResults), DispatchLevel::device },
    Entry{ "vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetRenderAreaGranularity), DispatchLevel::device },
    Entry{ "vkGetSemaphoreCounterValue", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetSemaphoreCounterValue), DispatchLevel::device },
    Entry{ "vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkInvalidateMappedMemoryRanges), DispatchLevel::device },
    Entry{ "vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkMergePipelineCaches), DispatchLevel::device },
    Entry{ "vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkQueueBindSparse), DispatchLevel::device },
    Entry{ "vkQueueSubmit2", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkQueueSubmit2), DispatchLevel::device },
    Entry{ "vkResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkResetQueryPool), DispatchLevel::device },
    Entry{ "vkSetPrivateData", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkSetPrivateData), DispatchLevel::device },
    Entry{ "vkSignalSemaphore", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkSignalSemaphore), DispatchLevel::device },
    Entry{ "vkTrimCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkTrimCommandPool), DispatchLevel::device },
    Entry{ "vkUpdateDescriptorSetWithTemplate", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkUpdateDescriptorSetWithTemplate), DispatchLevel::device },
    Entry{ "vkWaitSemaphores", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkWaitSemaphores), DispatchLevel::device },
    Entry{ "vkGetPhysicalDeviceFeatures2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFeatures2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceFormatProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFormatProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceImageFormatProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceImageFormatProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceMemoryProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceMemoryProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceQueueFamilyProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceQueueFamilyProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceSparseImageFormatProperties2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceSparseImageFormatProperties2KHR), DispatchLevel::physical_device },
    Entry{ "vkBindBufferMemory2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindBufferMemory2KHR), DispatchLevel::device },
    Entry{ "vkBindImageMemory2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBindImageMemory2KHR), DispatchLevel::device },
    Entry{ "vkGetBufferMemoryRequirements2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferMemoryRequirements2KHR), DispatchLevel::device },
    Entry{ "vkGetImageMemoryRequirements2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageMemoryRequirements2KHR), DispatchLevel::device },
    Entry{ "vkGetImageSparseMemoryRequirements2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSparseMemoryRequirements2KHR), DispatchLevel::device },
    Entry{ "vkCmdBeginRenderPass2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginRenderPass2KHR), DispatchLevel::device },
    Entry{ "vkCmdBeginRenderingKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginRenderingKHR), DispatchLevel::device },
    Entry{ "vkCmdBlitImage2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBlitImage2KHR), DispatchLevel::device },
    Entry{ "vkCmdCopyBuffer2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBuffer2KHR), DispatchLevel::device },
    Entry{ "vkCmdCopyBufferToImage2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyBufferToImage2KHR), DispatchLevel::device },
    Entry{ "vkCmdCopyImage2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImage2KHR), DispatchLevel::device },
    Entry{ "vkCmdCopyImageToBuffer2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyImageToBuffer2KHR), DispatchLevel::device },
    Entry{ "vkCmdDispatchBaseKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDispatchBaseKHR), DispatchLevel::device },
    Entry{ "vkCmdDrawIndexedIndirectCountKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndexedIndirectCountKHR), DispatchLevel::device },
    Entry{ "vkCmdDrawIndirectCountKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndirectCountKHR), DispatchLevel::device },
    Entry{ "vkCmdEndRenderPass2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndRenderPass2KHR), DispatchLevel::device },
    Entry{ "vkCmdEndRenderingKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndRenderingKHR), DispatchLevel::device },
    Entry{ "vkCmdNextSubpass2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdNextSubpass2KHR), DispatchLevel::device },
    Entry{ "vkCmdPipelineBarrier2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPipelineBarrier2KHR), DispatchLevel::device },
    Entry{ "vkCmdPushDescriptorSetWithTemplateKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPushDescriptorSetWithTemplateKHR), DispatchLevel::device },
    Entry{ "vkCmdResetEvent2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResetEvent2KHR), DispatchLevel::device },
    Entry{ "vkCmdResolveImage2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdResolveImage2KHR), DispatchLevel::device },
    Entry{ "vkCmdSetDeviceMaskKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDeviceMaskKHR), DispatchLevel::device },
    Entry{ "vkCmdSetEvent2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetEvent2KHR), DispatchLevel::device },
    Entry{ "vkCmdWaitEvents2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWaitEvents2KHR), DispatchLevel::device },
    Entry{ "vkCmdWriteTimestamp2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWriteTimestamp2KHR), DispatchLevel::device },
    Entry{ "vkCreateDescriptorUpdateTemplateKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDescriptorUpdateTemplateKHR), DispatchLevel::device },
    Entry{ "vkCreateRenderPass2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateRenderPass2KHR), DispatchLevel::device },
    Entry{ "vkCreateSamplerYcbcrConversionKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateSamplerYcbcrConversionKHR), DispatchLevel::device },
    Entry{ "vkDestroyDescriptorUpdateTemplateKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDescriptorUpdateTemplateKHR), DispatchLevel::device },
    Entry{ "vkDestroySamplerYcbcrConversionKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroySamplerYcbcrConversionKHR), DispatchLevel::device },
    Entry{ "vkEnumeratePhysicalDeviceGroupsKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkEnumeratePhysicalDeviceGroupsKHR), DispatchLevel::instance },
    Entry{ "vkGetBufferDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferDeviceAddressKHR), DispatchLevel::device },
    Entry{ "vkGetBufferOpaqueCaptureAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetBufferOpaqueCaptureAddressKHR), DispatchLevel::device },
    Entry{ "vkGetDescriptorSetLayoutSupportKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDescriptorSetLayoutSupportKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceBufferMemoryRequirementsKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceBufferMemoryRequirementsKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceGroupPeerMemoryFeaturesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceGroupPeerMemoryFeaturesKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceImageMemoryRequirementsKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceImageMemoryRequirementsKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceImageSparseMemoryRequirementsKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceImageSparseMemoryRequirementsKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceMemoryOpaqueCaptureAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceMemoryOpaqueCaptureAddressKHR), DispatchLevel::device },
    Entry{ "vkGetPhysicalDeviceExternalBufferPropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalBufferPropertiesKHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceExternalFencePropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalFencePropertiesKHR), DispatchLevel::physical_device },
    Entry{ "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR), DispatchLevel::physical_device },
    Entry{ "vkGetSemaphoreCounterValueKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetSemaphoreCounterValueKHR), DispatchLevel::device },
    Entry{ "vkQueueSubmit2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkQueueSubmit2KHR), DispatchLevel::device },
    Entry{ "vkSignalSemaphoreKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkSignalSemaphoreKHR), DispatchLevel::device },
    Entry{ "vkTrimCommandPoolKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkTrimCommandPoolKHR), DispatchLevel::device },
    Entry{ "vkUpdateDescriptorSetWithTemplateKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkUpdateDescriptorSetWithTemplateKHR), DispatchLevel::device },
    Entry{ "vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkWaitSemaphoresKHR), DispatchLevel::device },
    Entry{ "vkCmdBeginConditionalRenderingEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginConditionalRenderingEXT), DispatchLevel::device },
    Entry{ "vkCmdEndConditionalRenderingEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndConditionalRenderingEXT), DispatchLevel::device },
    Entry{ "vkCmdSetColorWriteEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetColorWriteEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBiasEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBiasEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetLogicOpEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLogicOpEXT), DispatchLevel::device },
    Entry{ "vkCmdSetPatchControlPointsEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPatchControlPointsEXT), DispatchLevel::device },
    Entry{ "vkCmdSetPrimitiveRestartEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPrimitiveRestartEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetRasterizerDiscardEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRasterizerDiscardEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetVertexInputEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetVertexInputEXT), DispatchLevel::device },
    Entry{ "vkBuildAccelerationStructuresKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkBuildAccelerationStructuresKHR), DispatchLevel::device },
    Entry{ "vkCmdBindShadersEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindShadersEXT), DispatchLevel::device },
    Entry{ "vkCmdBindVertexBuffers2EXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindVertexBuffers2EXT), DispatchLevel::device },
    Entry{ "vkCmdBuildAccelerationStructuresIndirectKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBuildAccelerationStructuresIndirectKHR), DispatchLevel::device },
    Entry{ "vkCmdBuildAccelerationStructuresKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBuildAccelerationStructuresKHR), DispatchLevel::device },
    Entry{ "vkCmdCopyAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkCmdCopyAccelerationStructureToMemoryKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyAccelerationStructureToMemoryKHR), DispatchLevel::device },
    Entry{ "vkCmdCopyMemoryToAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdCopyMemoryToAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkCmdDrawMeshTasksEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawMeshTasksEXT), DispatchLevel::device },
    Entry{ "vkCmdDrawMeshTasksIndirectCountEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawMeshTasksIndirectCountEXT), DispatchLevel::device },
    Entry{ "vkCmdDrawMeshTasksIndirectEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawMeshTasksIndirectEXT), DispatchLevel::device },
    Entry{ "vkCmdPushDescriptorSetKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdPushDescriptorSetKHR), DispatchLevel::device },
    Entry{ "vkCmdSetAlphaToCoverageEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetAlphaToCoverageEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetAlphaToOneEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetAlphaToOneEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetColorBlendAdvancedEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetColorBlendAdvancedEXT), DispatchLevel::device },
    Entry{ "vkCmdSetColorBlendEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetColorBlendEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetColorBlendEquationEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetColorBlendEquationEXT), DispatchLevel::device },
    Entry{ "vkCmdSetColorWriteMaskEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetColorWriteMaskEXT), DispatchLevel::device },
    Entry{ "vkCmdSetConservativeRasterizationModeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetConservativeRasterizationModeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageModulationModeNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageModulationModeNV), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageModulationTableEnableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageModulationTableEnableNV), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageModulationTableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageModulationTableNV), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageReductionModeNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageReductionModeNV), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageToColorEnableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageToColorEnableNV), DispatchLevel::device },
    Entry{ "vkCmdSetCoverageToColorLocationNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCoverageToColorLocationNV), DispatchLevel::device },
    Entry{ "vkCmdSetCullModeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetCullModeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthBoundsTestEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthBoundsTestEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthClampEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthClampEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthClampRangeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthClampRangeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthClipEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthClipEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthClipNegativeOneToOneEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthClipNegativeOneToOneEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthCompareOpEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthCompareOpEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthTestEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthTestEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetDepthWriteEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetDepthWriteEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetExtraPrimitiveOverestimationSizeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetExtraPrimitiveOverestimationSizeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetFragmentShadingRateKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetFragmentShadingRateKHR), DispatchLevel::device },
    Entry{ "vkCmdSetFrontFaceEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetFrontFaceEXT), DispatchLevel::device },
    Entry{ "vkCmdSetLineRasterizationModeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLineRasterizationModeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetLineStippleEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLineStippleEXT), DispatchLevel::device },
    Entry{ "vkCmdSetLineStippleEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLineStippleEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetLogicOpEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetLogicOpEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetPolygonModeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPolygonModeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetPrimitiveTopologyEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetPrimitiveTopologyEXT), DispatchLevel::device },
    Entry{ "vkCmdSetProvokingVertexModeEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetProvokingVertexModeEXT), DispatchLevel::device },
    Entry{ "vkCmdSetRasterizationSamplesEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRasterizationSamplesEXT), DispatchLevel::device },
    Entry{ "vkCmdSetRasterizationStreamEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRasterizationStreamEXT), DispatchLevel::device },
    Entry{ "vkCmdSetRayTracingPipelineStackSizeKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRayTracingPipelineStackSizeKHR), DispatchLevel::device },
    Entry{ "vkCmdSetRepresentativeFragmentTestEnableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRepresentativeFragmentTestEnableNV), DispatchLevel::device },
    Entry{ "vkCmdSetSampleLocationsEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetSampleLocationsEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetSampleMaskEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetSampleMaskEXT), DispatchLevel::device },
    Entry{ "vkCmdSetScissorWithCountEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetScissorWithCountEXT), DispatchLevel::device },
    Entry{ "vkCmdSetShadingRateImageEnableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetShadingRateImageEnableNV), DispatchLevel::device },
    Entry{ "vkCmdSetStencilOpEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilOpEXT), DispatchLevel::device },
    Entry{ "vkCmdSetStencilTestEnableEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetStencilTestEnableEXT), DispatchLevel::device },
    Entry{ "vkCmdSetTessellationDomainOriginEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetTessellationDomainOriginEXT), DispatchLevel::device },
    Entry{ "vkCmdSetViewportSwizzleNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetViewportSwizzleNV), DispatchLevel::device },
    Entry{ "vkCmdSetViewportWScalingEnableNV", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetViewportWScalingEnableNV), DispatchLevel::device },
    Entry{ "vkCmdSetViewportWithCountEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetViewportWithCountEXT), DispatchLevel::device },
    Entry{ "vkCmdTraceRaysIndirectKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdTraceRaysIndirectKHR), DispatchLevel::device },
    Entry{ "vkCmdTraceRaysKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdTraceRaysKHR), DispatchLevel::device },
    Entry{ "vkCmdWriteAccelerationStructuresPropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdWriteAccelerationStructuresPropertiesKHR), DispatchLevel::device },
    Entry{ "vkCopyAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkCopyAccelerationStructureToMemoryKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyAccelerationStructureToMemoryKHR), DispatchLevel::device },
    Entry{ "vkCopyImageToImageEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyImageToImageEXT), DispatchLevel::device },
    Entry{ "vkCopyImageToMemoryEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyImageToMemoryEXT), DispatchLevel::device },
    Entry{ "vkCopyMemoryToAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyMemoryToAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkCopyMemoryToImageEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCopyMemoryToImageEXT), DispatchLevel::device },
    Entry{ "vkCreateAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkCreateDeferredOperationKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateDeferredOperationKHR), DispatchLevel::device },
    Entry{ "vkCreateRayTracingPipelinesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateRayTracingPipelinesKHR), DispatchLevel::device },
    Entry{ "vkCreateShadersEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreateShadersEXT), DispatchLevel::device },
    Entry{ "vkDeferredOperationJoinKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDeferredOperationJoinKHR), DispatchLevel::device },
    Entry{ "vkDestroyAccelerationStructureKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyAccelerationStructureKHR), DispatchLevel::device },
    Entry{ "vkDestroyDeferredOperationKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyDeferredOperationKHR), DispatchLevel::device },
    Entry{ "vkDestroyShaderEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyShaderEXT), DispatchLevel::device },
    Entry{ "vkGetAccelerationStructureBuildSizesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetAccelerationStructureBuildSizesKHR), DispatchLevel::device },
    Entry{ "vkGetAccelerationStructureDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetAccelerationStructureDeviceAddressKHR), DispatchLevel::device },
    Entry{ "vkGetDeferredOperationMaxConcurrencyKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeferredOperationMaxConcurrencyKHR), DispatchLevel::device },
    Entry{ "vkGetDeferredOperationResultKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeferredOperationResultKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceAccelerationStructureCompatibilityKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceAccelerationStructureCompatibilityKHR), DispatchLevel::device },
    Entry{ "vkGetImageSubresourceLayout2EXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSubresourceLayout2EXT), DispatchLevel::device },
    Entry{ "vkGetPhysicalDeviceFragmentShadingRatesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceFragmentShadingRatesKHR), DispatchLevel::physical_device },
    Entry{ "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR), DispatchLevel::device },
    Entry{ "vkGetRayTracingShaderGroupHandlesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetRayTracingShaderGroupHandlesKHR), DispatchLevel::device },
    Entry{ "vkGetRayTracingShaderGroupStackSizeKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetRayTracingShaderGroupStackSizeKHR), DispatchLevel::device },
    Entry{ "vkTransitionImageLayoutEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkTransitionImageLayoutEXT), DispatchLevel::device },
    Entry{ "vkWriteAccelerationStructuresPropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkWriteAccelerationStructuresPropertiesKHR), DispatchLevel::device },
    Entry{ "vkGetCalibratedTimestampsEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetCalibratedTimestampsEXT), DispatchLevel::device },
    Entry{ "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT), DispatchLevel::physical_device },
    Entry{ "vkCmdSetRenderingAttachmentLocationsKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRenderingAttachmentLocationsKHR), DispatchLevel::device },
    Entry{ "vkCmdSetRenderingInputAttachmentIndicesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdSetRenderingInputAttachmentIndicesKHR), DispatchLevel::device },
    Entry{ "vkCmdBindIndexBuffer2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindIndexBuffer2KHR), DispatchLevel::device },
    Entry{ "vkCreatePipelineBinariesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCreatePipelineBinariesKHR), DispatchLevel::device },
    Entry{ "vkDestroyPipelineBinaryKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkDestroyPipelineBinaryKHR), DispatchLevel::device },
    Entry{ "vkGetDeviceImageSubresourceLayoutKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetDeviceImageSubresourceLayoutKHR), DispatchLevel::device },
    Entry{ "vkGetImageSubresourceLayout2KHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetImageSubresourceLayout2KHR), DispatchLevel::device },
    Entry{ "vkGetPipelineKeyKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetPipelineKeyKHR), DispatchLevel::device },
    Entry{ "vkGetRenderingAreaGranularityKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetRenderingAreaGranularityKHR), DispatchLevel::device },
    Entry{ "vkReleaseCapturedPipelineDataKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkReleaseCapturedPipelineDataKHR), DispatchLevel::device },
    Entry{ "vkCmdBeginQueryIndexedEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginQueryIndexedEXT), DispatchLevel::device },
    Entry{ "vkCmdBeginTransformFeedbackEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBeginTransformFeedbackEXT), DispatchLevel::device },
    Entry{ "vkCmdBindTransformFeedbackBuffersEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdBindTransformFeedbackBuffersEXT), DispatchLevel::device },
    Entry{ "vkCmdDrawIndirectByteCountEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdDrawIndirectByteCountEXT), DispatchLevel::device },
    Entry{ "vkCmdEndQueryIndexedEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndQueryIndexedEXT), DispatchLevel::device },
    Entry{ "vkCmdEndTransformFeedbackEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkCmdEndTransformFeedbackEXT), DispatchLevel::device },
    Entry{ "vkSetDeviceMemoryPriorityEXT", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkSetDeviceMemoryPriorityEXT), DispatchLevel::device },
    Entry{ "vkGetFenceFdKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetFenceFdKHR), DispatchLevel::device },
    Entry{ "vkGetMemoryFdKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetMemoryFdKHR), DispatchLevel::device },
    Entry{ "vkGetMemoryFdPropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetMemoryFdPropertiesKHR), DispatchLevel::device },
    Entry{ "vkGetSemaphoreFdKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkGetSemaphoreFdKHR), DispatchLevel::device },
    Entry{ "vkImportFenceFdKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkImportFenceFdKHR), DispatchLevel::device },
    Entry{ "vkImportSemaphoreFdKHR", reinterpret_cast<PFN_vkVoidFunction>(&Wrap_vkImportSemaphoreFdKHR), DispatchLevel::device },
};
}  // namespace

EntryTable::EntryTable(const Entry* first, size_t count) noexcept
    : first_(first), count_(count) {}

const Entry* EntryTable::begin() const noexcept { return first_; }
const Entry* EntryTable::end() const noexcept { return first_ + count_; }
size_t EntryTable::size() const noexcept { return count_; }

EntryTable wrapper_entries() noexcept { return EntryTable{kWrapperTable, 388u}; }

}  // namespace av::vkicd::generated
