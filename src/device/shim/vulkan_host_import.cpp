#include "device/shim/vulkan_host_import.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/probe_dma_buf.hpp"

#include <sys/mman.h>
#include <unistd.h>

namespace av {

VulkanHostImport::VulkanHostImport() : held_(0u)
{
    AV_SCOPE("VulkanHostImport::VulkanHostImport");
}

uint32_t VulkanHostImport::importable_types(VkDevice device, void *pointer)
{
    AV_SCOPE("VulkanHostImport::importable_types");
    /* Resolved through the device rather than linked, because a device made
     * without the extension has no such entry point and the loader says so. */
    auto *const properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
        ::vkGetDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT"));
    if (properties == nullptr) return 0u;

    VkMemoryHostPointerPropertiesEXT host = {};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    if (properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, pointer, &host) !=
        VK_SUCCESS)
        return 0u;
    return host.memoryTypeBits;
}

uint32_t VulkanHostImport::probe_types(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanHostImport::probe_types");
    static constexpr const char *const extensions[] = {"VK_KHR_external_memory", "VK_EXT_external_memory_host"};
    constexpr size_t                   page = 4096u;
    float                              priority = 1.0f;
    VkDeviceQueueCreateInfo            queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create.queueCreateInfoCount = 1u;
    create.pQueueCreateInfos = &queue;
    create.enabledExtensionCount = 2u;
    create.ppEnabledExtensionNames = extensions;
    VkDevice device = VK_NULL_HANDLE;
    if (::vkCreateDevice(physical, &create, nullptr, &device) != VK_SUCCESS) return 0u;
    void *const pointer = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    const uint32_t mask = pointer != MAP_FAILED ? importable_types(device, pointer) : 0u;
    if (pointer != MAP_FAILED) static_cast<void>(munmap(pointer, page));
    ::vkDestroyDevice(device, nullptr);
    return mask;
}

uint32_t VulkanHostImport::probe_dmabuf_types(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanHostImport::probe_dmabuf_types");
    ProbeDmaBuf shared;
    if (!shared.make()) return 0u;
    static constexpr const char *const extensions[] = {"VK_KHR_external_memory", "VK_KHR_external_memory_fd",
                                                       "VK_EXT_external_memory_dma_buf"};
    float                              priority = 1.0f;
    VkDeviceQueueCreateInfo            queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create.queueCreateInfoCount = 1u;
    create.pQueueCreateInfos = &queue;
    create.enabledExtensionCount = 3u;
    create.ppEnabledExtensionNames = extensions;
    VkDevice device = VK_NULL_HANDLE;
    if (::vkCreateDevice(physical, &create, nullptr, &device) != VK_SUCCESS) return 0u;
    const uint32_t mask = dmabuf_types(device, shared.handle());
    ::vkDestroyDevice(device, nullptr);
    return mask;
}

bool VulkanHostImport::imports(VkDevice device)
{
    AV_SCOPE("VulkanHostImport::imports");
    return ::vkGetDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT") != nullptr;
}

bool VulkanHostImport::import(VkDevice device, void *pointer, uint64_t size, uint32_t memory_type,
                              uint64_t &memory)
{
    AV_SCOPE("VulkanHostImport::import");
    VkImportMemoryHostPointerInfoEXT imported = {};
    imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
    imported.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    imported.pHostPointer = pointer;

    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &imported;
    allocate.allocationSize = size;
    allocate.memoryTypeIndex = memory_type;

    VkDeviceMemory made = VK_NULL_HANDLE;
    if (::vkAllocateMemory(device, &allocate, nullptr, &made) != VK_SUCCESS) return false;
    memory = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

uint32_t VulkanHostImport::dmabuf_types(VkDevice device, int fd)
{
    AV_SCOPE("VulkanHostImport::dmabuf_types");
    auto *const properties = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
        ::vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    if (properties == nullptr || fd < 0) return 0u;
    VkMemoryFdPropertiesKHR answered = {};
    answered.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    if (properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &answered) !=
        VK_SUCCESS)
        return 0u;
    return answered.memoryTypeBits;
}

bool VulkanHostImport::import_dmabuf(VkDevice device, int fd, uint64_t size, uint32_t memory_type,
                                     uint64_t &memory)
{
    AV_SCOPE("VulkanHostImport::import_dmabuf");
    if (fd < 0) return false;
    VkImportMemoryFdInfoKHR imported = {};
    imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    imported.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    imported.fd = fd;

    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &imported;
    allocate.allocationSize = size;
    allocate.memoryTypeIndex = memory_type;

    VkDeviceMemory made = VK_NULL_HANDLE;
    if (::vkAllocateMemory(device, &allocate, nullptr, &made) != VK_SUCCESS) return false;
    memory = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

void VulkanHostImport::release(VkDevice device, uint64_t memory)
{
    AV_SCOPE("VulkanHostImport::release");
    ::vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(memory), nullptr);
    held_--;
}

VkExternalMemoryHandleTypeFlags VulkanHostImport::buffer_host_types(VkPhysicalDevice physical,
                                                                   const VkBufferCreateInfo &create,
                                                                   VkExternalMemoryHandleTypeFlagBits type)
{
    AV_SCOPE("VulkanHostImport::buffer_host_types");
    if (physical == VK_NULL_HANDLE) return 0u;
    VkPhysicalDeviceExternalBufferInfo buffer = {};
    buffer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
    buffer.flags = create.flags;
    buffer.usage = create.usage;
    buffer.handleType = type;
    VkExternalBufferProperties answer = {};
    answer.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
    ::vkGetPhysicalDeviceExternalBufferProperties(physical, &buffer, &answer);
    return (answer.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0u
               ? static_cast<VkExternalMemoryHandleTypeFlags>(type)
               : 0u;
}

VkExternalMemoryHandleTypeFlags VulkanHostImport::image_host_types(VkPhysicalDevice physical,
                                                                  const VkImageCreateInfo &create,
                                                                  VkExternalMemoryHandleTypeFlagBits type)
{
    AV_SCOPE("VulkanHostImport::image_host_types");
    if (physical == VK_NULL_HANDLE) return 0u;
    VkPhysicalDeviceExternalImageFormatInfo external = {};
    external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    external.handleType = type;
    VkPhysicalDeviceImageFormatInfo2 format = {};
    format.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    format.pNext = &external;
    format.format = create.format;
    format.type = create.imageType;
    format.tiling = create.tiling;
    format.usage = create.usage;
    format.flags = create.flags;
    VkExternalImageFormatProperties answer = {};
    answer.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    properties.pNext = &answer;
    if (::vkGetPhysicalDeviceImageFormatProperties2(physical, &format, &properties) != VK_SUCCESS) return 0u;
    return (answer.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0u
               ? static_cast<VkExternalMemoryHandleTypeFlags>(type)
               : 0u;
}

bool VulkanHostImport::export_memory(VkDevice device, uint64_t memory, VkExternalMemoryHandleTypeFlagBits type,
                                     int &fd)
{
    AV_SCOPE("VulkanHostImport::export_memory");
    fd = -1;
    auto *const get = reinterpret_cast<PFN_vkGetMemoryFdKHR>(::vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    if (get == nullptr) return false;
    VkMemoryGetFdInfoKHR ask = {};
    ask.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    ask.memory = reinterpret_cast<VkDeviceMemory>(memory);
    ask.handleType = type;
    int made = -1;
    if (get(device, &ask, &made) != VK_SUCCESS || made < 0) return false;
    fd = made;
    return true;
}

bool VulkanHostImport::export_semaphore(VkDevice device, uint64_t semaphore, int &fd)
{
    AV_SCOPE("VulkanHostImport::export_semaphore");
    fd = -1;
    auto *const get =
        reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(::vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
    if (get == nullptr) return false;
    VkSemaphoreGetFdInfoKHR ask = {};
    ask.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    ask.semaphore = reinterpret_cast<VkSemaphore>(semaphore);
    ask.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    int made = -1;
    if (get(device, &ask, &made) != VK_SUCCESS || made < 0) return false;
    fd = made;
    return true;
}

uint32_t VulkanHostImport::shared_memory_types(VkDevice device, int fd, VkExternalMemoryHandleTypeFlagBits type)
{
    AV_SCOPE("VulkanHostImport::shared_memory_types");
    auto *const properties = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
        ::vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    if (properties == nullptr || fd < 0) return 0u;
    VkMemoryFdPropertiesKHR answered = {};
    answered.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    if (properties(device, type, fd, &answered) != VK_SUCCESS) return 0u;
    return answered.memoryTypeBits;
}

bool VulkanHostImport::import_shared_memory(VkDevice device, int fd, VkExternalMemoryHandleTypeFlagBits type,
                                            uint64_t size, uint32_t memory_type, uint64_t &memory)
{
    AV_SCOPE("VulkanHostImport::import_shared_memory");
    if (fd < 0) return false;
    const int copy = ::dup(fd);
    if (copy < 0) return false;
    VkImportMemoryFdInfoKHR imported = {};
    imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    imported.handleType = type;
    imported.fd = copy;

    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &imported;
    allocate.allocationSize = size;
    allocate.memoryTypeIndex = memory_type;

    VkDeviceMemory made = VK_NULL_HANDLE;
    if (::vkAllocateMemory(device, &allocate, nullptr, &made) != VK_SUCCESS) {
        ::close(copy);
        return false;
    }
    memory = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

bool VulkanHostImport::import_shared_semaphore(VkDevice device, int fd, uint64_t &semaphore)
{
    AV_SCOPE("VulkanHostImport::import_shared_semaphore");
    semaphore = 0u;
    auto *const import =
        reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(::vkGetDeviceProcAddr(device, "vkImportSemaphoreFdKHR"));
    if (import == nullptr || fd < 0) return false;
    const int copy = ::dup(fd);
    if (copy < 0) return false;

    VkSemaphoreTypeCreateInfo timeline = {};
    timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create.pNext = &timeline;
    VkSemaphore made = VK_NULL_HANDLE;
    if (::vkCreateSemaphore(device, &create, nullptr, &made) != VK_SUCCESS) {
        ::close(copy);
        return false;
    }
    VkImportSemaphoreFdInfoKHR imported = {};
    imported.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    imported.semaphore = made;
    imported.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    imported.fd = copy;
    if (import(device, &imported) != VK_SUCCESS) {
        ::vkDestroySemaphore(device, made, nullptr);
        ::close(copy);
        return false;
    }
    semaphore = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

void VulkanHostImport::destroy_semaphore(VkDevice device, uint64_t semaphore)
{
    AV_SCOPE("VulkanHostImport::destroy_semaphore");
    if (semaphore == 0u) return;
    ::vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(semaphore), nullptr);
    held_--;
}

uint32_t VulkanHostImport::held() const
{
    AV_SCOPE("VulkanHostImport::held");
    return held_;
}

}  // namespace av
