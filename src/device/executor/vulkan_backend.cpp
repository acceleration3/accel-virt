#include "device/executor/vulkan_backend.hpp"

#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>

namespace av {

namespace {

/* The executor whose handles the generated decoder is about to check. One
 * context runs on one worker thread and one call at a time - the ABI says so -
 * so the current backend is the thread's, and this is the only shape a hook
 * with no user pointer can take. */
thread_local VulkanBackend *inside = nullptr;

std::uint64_t number_of(const void *handle)
{
    std::uint64_t raw = 0u;
    memcpy(&raw, &handle, sizeof handle);
    return raw;
}

}  // namespace

VulkanBackend::VulkanBackend() : device_(VK_NULL_HANDLE), handles_(), ran_(0u), lost_(false) {}

bool VulkanBackend::knows_handle(uint64_t raw) const { return handles_.knows(raw); }

VulkanBackend *VulkanBackend::deciding() { return inside; }

VkResult VulkanBackend::on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *, VkInstance *pInstance)
{
    const VkResult result = RealVulkanDispatch::on_vkCreateInstance(pCreateInfo, nullptr, pInstance);
    if (result == VK_SUCCESS && pInstance != nullptr && !handles_.remember(number_of(*pInstance)))
        lost_ = true;
    return result;
}

void VulkanBackend::on_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *)
{
    handles_.forget(number_of(instance));
    RealVulkanDispatch::on_vkDestroyInstance(instance, nullptr);
}

VkResult VulkanBackend::on_vkEnumeratePhysicalDevices(VkInstance instance,
                                                      uint32_t *pPhysicalDeviceCount,
                                                      VkPhysicalDevice *pPhysicalDevices)
{
    const VkResult result = RealVulkanDispatch::on_vkEnumeratePhysicalDevices(
        instance, pPhysicalDeviceCount, pPhysicalDevices);
    if (pPhysicalDevices == nullptr || pPhysicalDeviceCount == nullptr) return result;
    for (uint32_t i = 0u; i < *pPhysicalDeviceCount; i++) {
        if (!handles_.remember(number_of(pPhysicalDevices[i]))) lost_ = true;
    }
    return result;
}

void VulkanBackend::on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                        uint32_t queueIndex, VkQueue *pQueue)
{
    RealVulkanDispatch::on_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    if (pQueue != nullptr && !handles_.remember(number_of(*pQueue))) lost_ = true;
}

VkResult VulkanBackend::on_vkAllocateCommandBuffers(VkDevice device,
                                                    const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                    VkCommandBuffer *pCommandBuffers)
{
    const VkResult result =
        RealVulkanDispatch::on_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    if (result != VK_SUCCESS || pCommandBuffers == nullptr || pAllocateInfo == nullptr)
        return result;
    for (uint32_t i = 0u; i < pAllocateInfo->commandBufferCount; i++) {
        if (!handles_.remember(number_of(pCommandBuffers[i]))) lost_ = true;
    }
    return result;
}

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
    inside = this;
    const bool decoded = vkproto::dispatch_command(*this, command_id, payload, reply, arena);
    inside = nullptr;
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
    if (result == VK_SUCCESS && pDevice != nullptr) {
        device_ = *pDevice;
        if (!handles_.remember(number_of(*pDevice))) lost_ = true;
    }
    return result;
}

void VulkanBackend::on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *)
{
    if (device == device_) device_ = VK_NULL_HANDLE;
    handles_.forget(number_of(device));
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


/* The generated decoder's inbound handle hook, defined here rather than in a
 * file of its own: the defaults are weak and live in the generated archive, and
 * a strong definition in an object nothing else references is never pulled in
 * to override them. Next to the class whose state it reads, it always is. */
namespace vkproto {

namespace {

template <typename H>
bool answer(const VulkanBackend *backend, uint64_t raw, H &out)
{
    /* Null is a legitimate value for an optional handle and never for one of
     * these five: the loader dereferences a dispatchable handle to find the
     * table to call through, so a guest naming the null instance aborts the
     * process rather than getting an error back. The fuzz target found that in
     * vkCmdResetEvent2 (feature 042). */
    if (raw == 0u) return false;
    if (backend == nullptr || !backend->knows_handle(raw)) return false;
    memcpy(&out, &raw, sizeof out);
    return true;
}

}  // namespace

bool wire_to_handle(uint64_t raw, VkInstance &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkPhysicalDevice &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkDevice &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkQueue &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

bool wire_to_handle(uint64_t raw, VkCommandBuffer &out) noexcept
{
    return answer(VulkanBackend::deciding(), raw, out);
}

}  // namespace vkproto

}  // namespace av
