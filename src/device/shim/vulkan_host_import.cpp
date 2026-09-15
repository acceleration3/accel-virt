#include "device/shim/vulkan_host_import.hpp"

namespace av {

VulkanHostImport::VulkanHostImport() : held_(0u) {}

uint32_t VulkanHostImport::importable_types(VkDevice device, void *pointer)
{
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

bool VulkanHostImport::import(VkDevice device, void *pointer, uint64_t size, uint32_t memory_type,
                              uint64_t &memory)
{
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

void VulkanHostImport::release(VkDevice device, uint64_t memory)
{
    ::vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(memory), nullptr);
    held_--;
}

uint32_t VulkanHostImport::held() const { return held_; }

}  // namespace av
