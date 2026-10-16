#include "executor/shim/vulkan_memory_import.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_device_table.hpp"

namespace av {

VulkanMemoryImport::VulkanMemoryImport(const VulkanDeviceTable &table) : table_(table), held_(0u)
{
    AV_SCOPE("VulkanMemoryImport::VulkanMemoryImport");
}

bool VulkanMemoryImport::imports() const
{
    AV_SCOPE("VulkanMemoryImport::imports");
    return table_.imports_dma_bufs();
}

uint32_t VulkanMemoryImport::dmabuf_types(int fd) const
{
    AV_SCOPE("VulkanMemoryImport::dmabuf_types");
    return table_.dma_buf_types(fd);
}

bool VulkanMemoryImport::import_dmabuf(OwnedFd &descriptor, uint64_t size, uint32_t memory_type, uint64_t &memory)
{
    AV_SCOPE("VulkanMemoryImport::import_dmabuf");
    VkDeviceMemory made = VK_NULL_HANDLE;
    if (!table_.import_memory(descriptor, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, size, memory_type, nullptr,
                              made))
        return false;
    memory = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

void VulkanMemoryImport::release(uint64_t memory)
{
    AV_SCOPE("VulkanMemoryImport::release");
    table_.free_memory(reinterpret_cast<VkDeviceMemory>(memory));
    held_--;
}

VkExternalMemoryHandleTypeFlags VulkanMemoryImport::buffer_external_types(VkPhysicalDevice physical,
                                                                   const VkBufferCreateInfo &create,
                                                                   VkExternalMemoryHandleTypeFlagBits type)
{
    AV_SCOPE("VulkanMemoryImport::buffer_external_types");
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

VkExternalMemoryHandleTypeFlags VulkanMemoryImport::image_external_types(VkPhysicalDevice physical,
                                                                  const VkImageCreateInfo &create,
                                                                  VkExternalMemoryHandleTypeFlagBits type)
{
    AV_SCOPE("VulkanMemoryImport::image_external_types");
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

bool VulkanMemoryImport::export_memory(uint64_t memory, VkExternalMemoryHandleTypeFlagBits type, OwnedFd &descriptor)
{
    AV_SCOPE("VulkanMemoryImport::export_memory");
    int        fd = -1;
    const bool exported = table_.memory_fd(reinterpret_cast<VkDeviceMemory>(memory), type, fd);
    descriptor.hold(fd);
    return exported;
}

bool VulkanMemoryImport::export_semaphore(uint64_t semaphore, OwnedFd &descriptor)
{
    AV_SCOPE("VulkanMemoryImport::export_semaphore");
    int        fd = -1;
    const bool exported =
        table_.semaphore_fd(reinterpret_cast<VkSemaphore>(semaphore), VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, fd);
    descriptor.hold(fd);
    return exported;
}

bool VulkanMemoryImport::import_shared_memory(int fd, VkExternalMemoryHandleTypeFlagBits type, uint64_t size,
                                              uint32_t memory_type, uint64_t &memory)
{
    AV_SCOPE("VulkanMemoryImport::import_shared_memory");
    OwnedFd        copy;
    VkDeviceMemory made = VK_NULL_HANDLE;
    if (!copy.hold_copy(fd) || !table_.import_memory(copy, type, size, memory_type, nullptr, made)) return false;
    memory = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

bool VulkanMemoryImport::import_shared_semaphore(int fd, uint64_t &semaphore)
{
    AV_SCOPE("VulkanMemoryImport::import_shared_semaphore");
    semaphore = 0u;
    OwnedFd copy;
    if (!copy.hold_copy(fd)) return false;
    const VkSemaphoreTypeCreateInfo timeline = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                                VK_SEMAPHORE_TYPE_TIMELINE, 0u};
    VkSemaphore made = VK_NULL_HANDLE;
    if (!table_.create_semaphore(&timeline, made)) return false;
    if (!table_.import_semaphore_fd(made, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, copy.fd())) {
        table_.destroy_semaphore(made);
        return false;
    }
    /* A successful import owns the descriptor. */
    static_cast<void>(copy.take());
    semaphore = reinterpret_cast<uint64_t>(made);
    held_++;
    return true;
}

void VulkanMemoryImport::destroy_semaphore(uint64_t semaphore)
{
    AV_SCOPE("VulkanMemoryImport::destroy_semaphore");
    if (semaphore == 0u) return;
    table_.destroy_semaphore(reinterpret_cast<VkSemaphore>(semaphore));
    held_--;
}

uint32_t VulkanMemoryImport::held() const
{
    AV_SCOPE("VulkanMemoryImport::held");
    return held_;
}

}  // namespace av
