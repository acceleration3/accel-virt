#include "device/executor/vulkan_backend.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>

namespace av {

VulkanBackend::VulkanBackend() : device_(VK_NULL_HANDLE), ran_(0u), lost_(false) {}

bool VulkanBackend::knows(uint32_t command_id) const
{
    if (command_id >> 16u != vulkan_api) return false;
    /* The generator writes one name per id it carries and "unknown" for the
     * rest, which is exactly the question being asked. Deciding it here rather
     * than by letting the dispatch fall off its switch keeps an id the profile
     * does not carry a skip instead of a fault. */
    return strcmp(vkproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool VulkanBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    const bool decoded = vkproto::dispatch_command(*this, command_id, payload, reply, arena);
    if (!decoded) return false;
    ran_++;
    return true;
}

VkResult VulkanBackend::on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                          const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkDevice *pDevice)
{
    const VkResult result = RealVulkanDispatch::on_vkCreateDevice(physicalDevice, pCreateInfo,
                                                                  nullptr, pDevice);
    /* Remembered because a VkDeviceMemory belongs to a device, and an import
     * arrives out of band with no command to carry one. A context makes one
     * device; a second replaces the first, which is what the guest sees too. */
    if (result == VK_SUCCESS && pDevice != nullptr) device_ = *pDevice;
    return result;
}

void VulkanBackend::on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *)
{
    if (device == device_) device_ = VK_NULL_HANDLE;
    RealVulkanDispatch::on_vkDestroyDevice(device, nullptr);
}

bool VulkanBackend::memory_type_for(void *pointer, uint32_t &type) const
{
    auto *const properties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(
        ::vkGetDeviceProcAddr(device_, "vkGetMemoryHostPointerPropertiesEXT"));
    if (properties == nullptr) return false;

    VkMemoryHostPointerPropertiesEXT host = {};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    if (properties(device_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, pointer,
                   &host) != VK_SUCCESS)
        return false;
    if (host.memoryTypeBits == 0u) return false;

    for (uint32_t candidate = 0u; candidate < 32u; candidate++) {
        if ((host.memoryTypeBits & (1u << candidate)) != 0u) {
            type = candidate;
            return true;
        }
    }
    return false;
}

bool VulkanBackend::adopt(ImportedBlob &blob)
{
    /* Only a host pointer, because that is the only kind this backend offers
     * through import_kinds, and a dma-buf imported here would land in memory
     * the host cannot map on one of the two GPUs measured. */
    if (blob.kind() != RVK_BLOB_HOST_POINTER || device_ == VK_NULL_HANDLE) return false;

    uint32_t type = 0u;
    if (!memory_type_for(blob.pointer(), type)) return false;

    VkImportMemoryHostPointerInfoEXT imported = {};
    imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
    imported.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    imported.pHostPointer = blob.pointer();

    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &imported;
    allocate.allocationSize = blob.size();
    allocate.memoryTypeIndex = type;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (::vkAllocateMemory(device_, &allocate, nullptr, &memory) != VK_SUCCESS) return false;
    blob.set_object(reinterpret_cast<uint64_t>(memory));
    return true;
}

void VulkanBackend::abandon(ImportedBlob &blob)
{
    const uint64_t object = blob.object();
    if (object == 0u || device_ == VK_NULL_HANDLE) return;
    /* The pointer itself is the device's and stays mapped; what this made of
     * it is this library's to free. */
    ::vkFreeMemory(device_, reinterpret_cast<VkDeviceMemory>(object), nullptr);
    blob.set_object(0u);
}

bool VulkanBackend::lost() const { return lost_; }

uint64_t VulkanBackend::ran() const { return ran_; }

}  // namespace av
