// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_dispatch_client.h"

#include "proto/wire/wire_writer.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

void rvk_vkDestroyInstance(::av::ClientTransport& t, VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyInstance, payload.bytes());
}

void rvk_vkDestroyDevice(::av::ClientTransport& t, VkDevice device, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDevice, payload.bytes());
}

void rvk_vkGetDeviceQueue(::av::ClientTransport& t, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, std::uint64_t* pQueue) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(queueFamilyIndex);
    payload.write_pod(queueIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueue)) {
        return;
    }
}

VkResult rvk_vkEnumeratePhysicalDevices(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceCount, std::uint64_t* pPhysicalDevices) {
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDevices != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDevices, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPhysicalDeviceCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPhysicalDeviceCount; ++k) {
            if (!r.read_pod(pPhysicalDevices[k])) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceProperties2(r, pProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkCreateInstance(::av::ClientTransport& t, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, std::uint64_t* pInstance) {
    ::av::WireWriter payload;
    serialize_VkInstanceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateInstance, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pInstance)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDevice(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, std::uint64_t* pDevice) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkDeviceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDevice, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDevice)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceProperties(r, pProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFeatures(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceFeatures(r, pFeatures, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkFormatProperties(r, pFormatProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(tiling);
    payload.write_pod(usage);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkImageFormatProperties(r, pImageFormatProperties, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceMemoryProperties(r, pMemoryProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueueFamilyPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pQueueFamilyPropertyCount; ++k) {
            if (!deserialize_VkQueueFamilyProperties(r, &pQueueFamilyProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(samples);
    payload.write_pod(usage);
    payload.write_pod(tiling);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPropertyCount; ++k) {
            if (!deserialize_VkSparseImageFormatProperties(r, &pProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

VkResult rvk_vkEnumerateDeviceExtensionProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    {
        const char* s = pLayerName;
        if (s == nullptr) {
            payload.write_varint(0);
        } else {
            const std::size_t n = std::strlen(s) + 1u;
            payload.write_varint(n);
            payload.write(s, n);
        }
    }
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceExtensionProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPropertyCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPropertyCount; ++k) {
            if (!deserialize_VkExtensionProperties(r, &pProperties[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkEnumerateDeviceLayerProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceLayerProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPropertyCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPropertyCount; ++k) {
            if (!deserialize_VkLayerProperties(r, &pProperties[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceFeatures2(r, pFeatures, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkFormatProperties2(r, pFormatProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkImageFormatProperties2(r, pImageFormatProperties, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, pMemoryProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueueFamilyPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pQueueFamilyPropertyCount; ++k) {
            if (!deserialize_VkQueueFamilyProperties2(r, &pQueueFamilyProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPropertyCount; ++k) {
            if (!deserialize_VkSparseImageFormatProperties2(r, &pProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

VkResult rvk_vkDeviceWaitIdle(::av::ClientTransport& t, VkDevice device) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkDeviceWaitIdle, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueWaitIdle(::av::ClientTransport& t, VkQueue queue) {
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueWaitIdle, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateMemory(::av::ClientTransport& t, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryAllocateInfo(payload, *pAllocateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateMemory, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pMemory)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkFreeMemory(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkFreeMemory, payload.bytes());
}

VkResult rvk_vkBindBufferMemory(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory(::av::ClientTransport& t, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

VkResult rvk_vkCreateBuffer(::av::ClientTransport& t, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBuffer, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pBuffer)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBuffer(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyBuffer, payload.bytes());
}

VkResult rvk_vkCreateImage(::av::ClientTransport& t, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImage, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pImage)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImage(::av::ClientTransport& t, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyImage, payload.bytes());
}

VkResult rvk_vkCreateImageView(::av::ClientTransport& t, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImageView, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pView)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImageView(::av::ClientTransport& t, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageView(payload, imageView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyImageView, payload.bytes());
}

VkResult rvk_vkCreateSampler(::av::ClientTransport& t, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSampler, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSampler)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySampler(::av::ClientTransport& t, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSampler(payload, sampler);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySampler, payload.bytes());
}

VkResult rvk_vkCreateSemaphore(::av::ClientTransport& t, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSemaphore, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSemaphore)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySemaphore(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySemaphore, payload.bytes());
}

VkResult rvk_vkCreateFence(::av::ClientTransport& t, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFenceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFence, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFence)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFence(::av::ClientTransport& t, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyFence, payload.bytes());
}

VkResult rvk_vkResetFences(::av::ClientTransport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(fenceCount); ++rvk_i) {
        serialize_VkFence(payload, pFences[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkResetFences, payload.bytes());
    return VK_SUCCESS;
}

VkResult rvk_vkWaitForFences(::av::ClientTransport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(fenceCount); ++rvk_i) {
        serialize_VkFence(payload, pFences[rvk_i]);
    }
    payload.write_pod(waitAll);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitForFences, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetFenceStatus(::av::ClientTransport& t, VkDevice device, VkFence fence) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceStatus, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateCommandPool(::av::ClientTransport& t, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateCommandPool, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pCommandPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyCommandPool, payload.bytes());
}

VkResult rvk_vkResetCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkResetCommandPool, payload.bytes());
    return VK_SUCCESS;
}

VkResult rvk_vkBeginCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCommandBufferBeginInfo(payload, *pBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkBeginCommandBuffer, payload.bytes());
    return VK_SUCCESS;
}

VkResult rvk_vkEndCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkEndCommandBuffer, payload.bytes());
    return VK_SUCCESS;
}

VkResult rvk_vkResetCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkResetCommandBuffer, payload.bytes());
    return VK_SUCCESS;
}

void rvk_vkDestroyShaderModule(::av::ClientTransport& t, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderModule(payload, shaderModule);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyShaderModule, payload.bytes());
}

VkResult rvk_vkCreateShaderModule(::av::ClientTransport& t, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderModuleCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShaderModule, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pShaderModule)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePipelineLayout(::av::ClientTransport& t, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineLayout, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPipelineLayout)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineLayout(::av::ClientTransport& t, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayout(payload, pipelineLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineLayout, payload.bytes());
}

void rvk_vkDestroyPipeline(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipeline, payload.bytes());
}

VkResult rvk_vkCreatePipelineCache(::av::ClientTransport& t, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCacheCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineCache, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPipelineCache)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineCache(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineCache, payload.bytes());
}

VkResult rvk_vkCreateRenderPass(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyRenderPass(::av::ClientTransport& t, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyRenderPass, payload.bytes());
}

VkResult rvk_vkCreateFramebuffer(::av::ClientTransport& t, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFramebufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFramebuffer, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFramebuffer)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFramebuffer(::av::ClientTransport& t, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFramebuffer(payload, framebuffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyFramebuffer, payload.bytes());
}

VkResult rvk_vkCreateDescriptorSetLayout(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorSetLayout, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSetLayout)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorSetLayout(::av::ClientTransport& t, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayout(payload, descriptorSetLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorSetLayout, payload.bytes());
}

VkResult rvk_vkCreateDescriptorPool(::av::ClientTransport& t, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorPool, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorPool(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorPool, payload.bytes());
}

void rvk_vkFreeCommandBuffers(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(commandBufferCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(commandBufferCount); ++rvk_i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkFreeCommandBuffers, payload.bytes());
}

VkResult rvk_vkQueueSubmit(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    (void)t.send_async(::av::vkproto::kCmd_vkQueueSubmit, payload.bytes());
    return VK_SUCCESS;
}

void rvk_vkCmdBindPipeline(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipeline(payload, pipeline);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindPipeline, payload.bytes());
}

void rvk_vkCmdSetViewport(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewport, payload.bytes());
}

void rvk_vkCmdSetScissor(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstScissor);
    payload.write_pod(scissorCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissor, payload.bytes());
}

void rvk_vkCmdBindVertexBuffers(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers, payload.bytes());
}

void rvk_vkCmdBindIndexBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(indexType);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer, payload.bytes());
}

void rvk_vkCmdBindDescriptorSets(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(firstSet);
    payload.write_pod(descriptorSetCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(descriptorSetCount); ++rvk_i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[rvk_i]);
    }
    payload.write_pod(dynamicOffsetCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(dynamicOffsetCount); ++rvk_i) {
        payload.write_pod(pDynamicOffsets[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorSets, payload.bytes());
}

void rvk_vkCmdDraw(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstVertex);
    payload.write_pod(firstInstance);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDraw, payload.bytes());
}

void rvk_vkCmdDrawIndexed(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(indexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstIndex);
    payload.write_pod(vertexOffset);
    payload.write_pod(firstInstance);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexed, payload.bytes());
}

void rvk_vkCmdPipelineBarrier(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(dependencyFlags);
    payload.write_pod(memoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(memoryBarrierCount); ++rvk_i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[rvk_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++rvk_i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[rvk_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++rvk_i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier, payload.bytes());
}

void rvk_vkCmdCopyBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferCopy(payload, pRegions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer, payload.bytes());
}

void rvk_vkCmdBeginRenderPass(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    payload.write_pod(contents);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass, payload.bytes());
}

void rvk_vkCmdEndRenderPass(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass, payload.bytes());
}

void rvk_vkCmdNextSubpass(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(contents);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass, payload.bytes());
}

void rvk_vkCmdPushConstants(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(stageFlags);
    payload.write_pod(offset);
    payload.write_pod(size);
    if (pValues != nullptr && size > 0) {
        payload.write(pValues, static_cast<std::size_t>(size));
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPushConstants, payload.bytes());
}

VkResult rvk_vkCreateGraphicsPipelines(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkGraphicsPipelineCreateInfo(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateGraphicsPipelines, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkUpdateDescriptorSets(::av::ClientTransport& t, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(descriptorWriteCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(descriptorWriteCount); ++rvk_i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[rvk_i]);
    }
    payload.write_pod(descriptorCopyCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(descriptorCopyCount); ++rvk_i) {
        serialize_VkCopyDescriptorSet(payload, pDescriptorCopies[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSets, payload.bytes());
}

VkResult rvk_vkAllocateCommandBuffers(::av::ClientTransport& t, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, std::uint64_t* pCommandBuffers) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandBufferAllocateInfo(payload, *pAllocateInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateCommandBuffers, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(pAllocateInfo->commandBufferCount); ++rvk_k) {
        if (!r.read_pod(pCommandBuffers[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateDescriptorSets(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetAllocateInfo(payload, *pAllocateInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateDescriptorSets, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(pAllocateInfo->descriptorSetCount); ++rvk_k) {
        if (!r.read_pod(pDescriptorSets[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFreeDescriptorSets(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(descriptorSetCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(descriptorSetCount); ++rvk_i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkFreeDescriptorSets, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateComputePipelines(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkComputePipelineCreateInfo(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateComputePipelines, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetDescriptorPool(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkResetDescriptorPool, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateEvent(::av::ClientTransport& t, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEventCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateEvent, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pEvent)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyEvent(::av::ClientTransport& t, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyEvent, payload.bytes());
}

VkResult rvk_vkGetEventStatus(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetEventStatus, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSetEvent(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSetEvent, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetEvent(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkResetEvent, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetEvent(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent, payload.bytes());
}

void rvk_vkCmdResetEvent(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent, payload.bytes());
}

void rvk_vkCmdWaitEvents(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(memoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(memoryBarrierCount); ++rvk_i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[rvk_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++rvk_i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[rvk_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++rvk_i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents, payload.bytes());
}

void rvk_vkCmdCopyImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageCopy(payload, pRegions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage, payload.bytes());
}

void rvk_vkCmdCopyBufferToImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferImageCopy(payload, pRegions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage, payload.bytes());
}

void rvk_vkCmdCopyImageToBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferImageCopy(payload, pRegions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer, payload.bytes());
}

void rvk_vkCmdBlitImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageBlit(payload, pRegions[rvk_i]);
    }
    payload.write_pod(filter);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage, payload.bytes());
}

void rvk_vkCmdClearColorImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearColorValue(payload, *pColor);
    payload.write_pod(rangeCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(rangeCount); ++rvk_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearColorImage, payload.bytes());
}

void rvk_vkCmdClearDepthStencilImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearDepthStencilValue(payload, *pDepthStencil);
    payload.write_pod(rangeCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(rangeCount); ++rvk_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearDepthStencilImage, payload.bytes());
}

void rvk_vkCmdClearAttachments(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkClearAttachment(payload, pAttachments[rvk_i]);
    }
    payload.write_pod(rectCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(rectCount); ++rvk_i) {
        serialize_VkClearRect(payload, pRects[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearAttachments, payload.bytes());
}

void rvk_vkCmdFillBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(size);
    payload.write_pod(data);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdFillBuffer, payload.bytes());
}

void rvk_vkCmdUpdateBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(dataSize);
    if (pData != nullptr && dataSize > 0) {
        payload.write(pData, static_cast<std::size_t>(dataSize));
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdUpdateBuffer, payload.bytes());
}

void rvk_vkCmdSetLineWidth(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float lineWidth) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineWidth);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineWidth, payload.bytes());
}

void rvk_vkCmdSetDepthBias(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasConstantFactor);
    payload.write_pod(depthBiasClamp);
    payload.write_pod(depthBiasSlopeFactor);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBias, payload.bytes());
}

void rvk_vkCmdSetBlendConstants(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const float* blendConstants) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    for (std::uint32_t rvk_i = 0; rvk_i < 4u; ++rvk_i) {
        payload.write_pod(blendConstants[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetBlendConstants, payload.bytes());
}

void rvk_vkCmdSetDepthBounds(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(minDepthBounds);
    payload.write_pod(maxDepthBounds);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBounds, payload.bytes());
}

void rvk_vkCmdSetStencilCompareMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(compareMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilCompareMask, payload.bytes());
}

void rvk_vkCmdSetStencilWriteMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(writeMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilWriteMask, payload.bytes());
}

void rvk_vkCmdSetStencilReference(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(reference);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilReference, payload.bytes());
}

void rvk_vkCmdDispatch(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatch, payload.bytes());
}

void rvk_vkCmdDispatchIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchIndirect, payload.bytes());
}

void rvk_vkCmdDrawIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirect, payload.bytes());
}

void rvk_vkCmdDrawIndexedIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirect, payload.bytes());
}

void rvk_vkCmdExecuteCommands(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(commandBufferCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(commandBufferCount); ++rvk_i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdExecuteCommands, payload.bytes());
}

VkResult rvk_vkAcquireNextImage2KHR(::av::ClientTransport& t, VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAcquireNextImageInfoKHR(payload, *pAcquireInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAcquireNextImage2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pImageIndex)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindBufferMemory2(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQuery(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginQuery, payload.bytes());
}

void rvk_vkCmdBeginRenderPass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2, payload.bytes());
}

void rvk_vkCmdBeginRendering(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRendering, payload.bytes());
}

void rvk_vkCmdBindVertexBuffers2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    const std::uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pStrides[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2, payload.bytes());
}

void rvk_vkCmdBlitImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2, payload.bytes());
}

void rvk_vkCmdCopyBuffer2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2, payload.bytes());
}

void rvk_vkCmdCopyBufferToImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2, payload.bytes());
}

void rvk_vkCmdCopyImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2, payload.bytes());
}

void rvk_vkCmdCopyImageToBuffer2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2, payload.bytes());
}

void rvk_vkCmdCopyQueryPoolResults(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(stride);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyQueryPoolResults, payload.bytes());
}

void rvk_vkCmdDispatchBase(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchBase, payload.bytes());
}

void rvk_vkCmdDrawIndexedIndirectCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount, payload.bytes());
}

void rvk_vkCmdDrawIndirectCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCount, payload.bytes());
}

void rvk_vkCmdEndQuery(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndQuery, payload.bytes());
}

void rvk_vkCmdEndRenderPass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2, payload.bytes());
}

void rvk_vkCmdEndRendering(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRendering, payload.bytes());
}

void rvk_vkCmdNextSubpass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2, payload.bytes());
}

void rvk_vkCmdPipelineBarrier2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2, payload.bytes());
}

void rvk_vkCmdResetEvent2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2, payload.bytes());
}

void rvk_vkCmdResetQueryPool(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetQueryPool, payload.bytes());
}

void rvk_vkCmdResolveImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageResolve(payload, pRegions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage, payload.bytes());
}

void rvk_vkCmdResolveImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2, payload.bytes());
}

void rvk_vkCmdSetCullMode(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCullMode, payload.bytes());
}

void rvk_vkCmdSetDepthBiasEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnable, payload.bytes());
}

void rvk_vkCmdSetDepthBoundsTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable, payload.bytes());
}

void rvk_vkCmdSetDepthCompareOp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOp, payload.bytes());
}

void rvk_vkCmdSetDepthTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnable, payload.bytes());
}

void rvk_vkCmdSetDepthWriteEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnable, payload.bytes());
}

void rvk_vkCmdSetDeviceMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMask, payload.bytes());
}

void rvk_vkCmdSetEvent2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2, payload.bytes());
}

void rvk_vkCmdSetFrontFace(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFace, payload.bytes());
}

void rvk_vkCmdSetPrimitiveRestartEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable, payload.bytes());
}

void rvk_vkCmdSetPrimitiveTopology(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopology, payload.bytes());
}

void rvk_vkCmdSetRasterizerDiscardEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable, payload.bytes());
}

void rvk_vkCmdSetScissorWithCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCount, payload.bytes());
}

void rvk_vkCmdSetStencilOp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOp, payload.bytes());
}

void rvk_vkCmdSetStencilTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnable, payload.bytes());
}

void rvk_vkCmdSetViewportWithCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCount, payload.bytes());
}

void rvk_vkCmdWaitEvents2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(eventCount); ++rvk_i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2, payload.bytes());
}

void rvk_vkCmdWriteTimestamp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp, payload.bytes());
}

void rvk_vkCmdWriteTimestamp2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2, payload.bytes());
}

VkResult rvk_vkCreateBufferView(::av::ClientTransport& t, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBufferView, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pView)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDescriptorUpdateTemplate(::av::ClientTransport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplate, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorUpdateTemplate)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePrivateDataSlot(::av::ClientTransport& t, VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlotCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePrivateDataSlot, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPrivateDataSlot)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateQueryPool(::av::ClientTransport& t, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateQueryPool, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pQueryPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversion(::av::ClientTransport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversion, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pYcbcrConversion)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBufferView(::av::ClientTransport& t, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferView(payload, bufferView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyBufferView, payload.bytes());
}

void rvk_vkDestroyDescriptorUpdateTemplate(::av::ClientTransport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate, payload.bytes());
}

void rvk_vkDestroyPrivateDataSlot(::av::ClientTransport& t, VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPrivateDataSlot, payload.bytes());
}

void rvk_vkDestroyQueryPool(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyQueryPool, payload.bytes());
}

void rvk_vkDestroySamplerYcbcrConversion(::av::ClientTransport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion, payload.bytes());
}

VkResult rvk_vkEnumeratePhysicalDeviceGroups(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceGroupCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroups, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPhysicalDeviceGroupCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPhysicalDeviceGroupCount; ++k) {
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, &pPhysicalDeviceGroupProperties[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFlushMappedMemoryRanges(::av::ClientTransport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(memoryRangeCount); ++rvk_i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkFlushMappedMemoryRanges, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddress(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddress, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkDeviceAddress>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddress(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddress, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupport(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkDescriptorSetLayoutSupport(r, pSupport, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceBufferMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceGroupPeerMemoryFeatures(::av::ClientTransport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeatures, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPeerMemoryFeatures)) {
        return;
    }
}

VkResult rvk_vkGetDeviceGroupPresentCapabilitiesKHR(::av::ClientTransport& t, VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(pDeviceGroupPresentCapabilities->sType);
    serialize_pnext_chain(payload, pDeviceGroupPresentCapabilities->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkDeviceGroupPresentCapabilitiesKHR(r, pDeviceGroupPresentCapabilities, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetDeviceGroupSurfacePresentModesKHR(::av::ClientTransport& t, VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSurfaceKHR(payload, surface);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupSurfacePresentModesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pModes)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceImageMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceImageSparseMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pSparseMemoryRequirementCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pSparseMemoryRequirementCount; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetDeviceMemoryCommitment(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryCommitment, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pCommittedMemoryInBytes)) {
        return;
    }
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddress(::av::ClientTransport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDeviceQueue2(::av::ClientTransport& t, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, std::uint64_t* pQueue) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceQueueInfo2(payload, *pQueueInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueue)) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetImageSparseMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pSparseMemoryRequirementCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pSparseMemoryRequirementCount; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements(r, &pSparseMemoryRequirements[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetImageSparseMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pSparseMemoryRequirementCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pSparseMemoryRequirementCount; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetImageSubresourceLayout(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource(payload, *pSubresource);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkSubresourceLayout(r, pLayout, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalBufferProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalBufferProperties(r, pExternalBufferProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalFenceProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalFenceProperties(r, pExternalFenceProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalSemaphoreProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalSemaphoreProperties(r, pExternalSemaphoreProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDevicePresentRectanglesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkSurfaceKHR(payload, surface);
    const std::uint8_t array_present = pRects != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pRectCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDevicePresentRectanglesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRectCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pRectCount; ++k) {
            if (!deserialize_VkRect2D(r, &pRects[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetPhysicalDeviceToolProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pToolProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pToolCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceToolProperties, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pToolCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pToolCount; ++k) {
            if (!deserialize_VkPhysicalDeviceToolProperties(r, &pToolProperties[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPrivateData(::av::ClientTransport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPrivateData, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pData)) {
        return;
    }
}

VkResult rvk_vkGetQueryPoolResults(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    payload.write_pod<std::uint64_t>(dataSize);
    payload.write_pod(stride);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetQueryPoolResults, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<std::size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderAreaGranularity(::av::ClientTransport& t, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderAreaGranularity, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExtent2D(r, pGranularity, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetSemaphoreCounterValue(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValue, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pValue)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkInvalidateMappedMemoryRanges(::av::ClientTransport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(memoryRangeCount); ++rvk_i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkInvalidateMappedMemoryRanges, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkMergePipelineCaches(::av::ClientTransport& t, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, dstCache);
    payload.write_pod(srcCacheCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(srcCacheCount); ++rvk_i) {
        serialize_VkPipelineCache(payload, pSrcCaches[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkMergePipelineCaches, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueBindSparse(::av::ClientTransport& t, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindSparseInfo(payload, pBindInfo[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueBindSparse, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo2(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    (void)t.send_async(::av::vkproto::kCmd_vkQueueSubmit2, payload.bytes());
    return VK_SUCCESS;
}

void rvk_vkResetQueryPool(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    (void)t.send_async(::av::vkproto::kCmd_vkResetQueryPool, payload.bytes());
}

VkResult rvk_vkSetPrivateData(::av::ClientTransport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    payload.write_pod(data);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSetPrivateData, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphore(::av::ClientTransport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphore, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkTrimCommandPool, payload.bytes());
}

VkResult rvk_vkWaitSemaphores(::av::ClientTransport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphores, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceFeatures2(r, pFeatures, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkFormatProperties2(r, pFormatProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkImageFormatProperties2(r, pImageFormatProperties, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, pMemoryProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPhysicalDeviceProperties2(r, pProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueueFamilyPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pQueueFamilyPropertyCount; ++k) {
            if (!deserialize_VkQueueFamilyProperties2(r, &pQueueFamilyProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPropertyCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPropertyCount; ++k) {
            if (!deserialize_VkSparseImageFormatProperties2(r, &pProperties[k], arena_arr)) {
            return;
            }
        }
    }
}

VkResult rvk_vkBindBufferMemory2KHR(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2KHR(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetImageSparseMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pSparseMemoryRequirementCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pSparseMemoryRequirementCount; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], arena_arr)) {
            return;
            }
        }
    }
}

void rvk_vkCmdBeginRenderPass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR, payload.bytes());
}

void rvk_vkCmdBeginRenderingKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderingKHR, payload.bytes());
}

void rvk_vkCmdBlitImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2KHR, payload.bytes());
}

void rvk_vkCmdCopyBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2KHR, payload.bytes());
}

void rvk_vkCmdCopyBufferToImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR, payload.bytes());
}

void rvk_vkCmdCopyImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2KHR, payload.bytes());
}

void rvk_vkCmdCopyImageToBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR, payload.bytes());
}

void rvk_vkCmdDispatchBaseKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchBaseKHR, payload.bytes());
}

void rvk_vkCmdDrawIndexedIndirectCountKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR, payload.bytes());
}

void rvk_vkCmdDrawIndirectCountKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR, payload.bytes());
}

void rvk_vkCmdEndRenderPass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2KHR, payload.bytes());
}

void rvk_vkCmdEndRenderingKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderingKHR, payload.bytes());
}

void rvk_vkCmdNextSubpass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2KHR, payload.bytes());
}

void rvk_vkCmdPipelineBarrier2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR, payload.bytes());
}

void rvk_vkCmdResetEvent2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2KHR, payload.bytes());
}

void rvk_vkCmdResolveImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2KHR, payload.bytes());
}

void rvk_vkCmdSetDeviceMaskKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR, payload.bytes());
}

void rvk_vkCmdSetEvent2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2KHR, payload.bytes());
}

void rvk_vkCmdWaitEvents2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(eventCount); ++rvk_i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2KHR, payload.bytes());
}

void rvk_vkCmdWriteTimestamp2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR, payload.bytes());
}

VkResult rvk_vkCreateDescriptorUpdateTemplateKHR(::av::ClientTransport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplateKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorUpdateTemplate)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2KHR(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversionKHR(::av::ClientTransport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversionKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pYcbcrConversion)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorUpdateTemplateKHR(::av::ClientTransport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR, payload.bytes());
}

void rvk_vkDestroySamplerYcbcrConversionKHR(::av::ClientTransport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR, payload.bytes());
}

VkResult rvk_vkEnumeratePhysicalDeviceGroupsKHR(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceGroupCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroupsKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPhysicalDeviceGroupCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pPhysicalDeviceGroupCount; ++k) {
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, &pPhysicalDeviceGroupProperties[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddressKHR(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddressKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkDeviceAddress>(result_code);
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddressKHR(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupportKHR(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkDescriptorSetLayoutSupport(r, pSupport, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceBufferMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceGroupPeerMemoryFeaturesKHR(::av::ClientTransport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeaturesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPeerMemoryFeatures)) {
        return;
    }
}

void rvk_vkGetDeviceImageMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, arena_out)) {
        return;
    }
}

void rvk_vkGetDeviceImageSparseMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirementsKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pSparseMemoryRequirementCount)) {
        return;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pSparseMemoryRequirementCount; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], arena_arr)) {
            return;
            }
        }
    }
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddressKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetPhysicalDeviceExternalBufferPropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalBufferProperties(r, pExternalBufferProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalFencePropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalFenceProperties(r, pExternalFenceProperties, arena_out)) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExternalSemaphoreProperties(r, pExternalSemaphoreProperties, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetSemaphoreCounterValueKHR(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValueKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pValue)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2KHR(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo2(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueSubmit2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphoreKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphoreKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPoolKHR(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkTrimCommandPoolKHR, payload.bytes());
}

VkResult rvk_vkWaitSemaphoresKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphoresKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginConditionalRenderingEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkConditionalRenderingBeginInfoEXT(payload, *pConditionalRenderingBegin);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT, payload.bytes());
}

void rvk_vkCmdEndConditionalRenderingEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT, payload.bytes());
}

void rvk_vkCmdSetColorWriteEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorWriteEnables[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT, payload.bytes());
}

void rvk_vkCmdSetDepthBiasEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT, payload.bytes());
}

void rvk_vkCmdSetLogicOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEXT, payload.bytes());
}

void rvk_vkCmdSetPatchControlPointsEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(patchControlPoints);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT, payload.bytes());
}

void rvk_vkCmdSetPrimitiveRestartEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT, payload.bytes());
}

void rvk_vkCmdSetRasterizerDiscardEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT, payload.bytes());
}

void rvk_vkCmdSetVertexInputEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexBindingDescriptionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(vertexBindingDescriptionCount); ++rvk_i) {
        serialize_VkVertexInputBindingDescription2EXT(payload, pVertexBindingDescriptions[rvk_i]);
    }
    payload.write_pod(vertexAttributeDescriptionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(vertexAttributeDescriptionCount); ++rvk_i) {
        serialize_VkVertexInputAttributeDescription2EXT(payload, pVertexAttributeDescriptions[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetVertexInputEXT, payload.bytes());
}

VkResult rvk_vkBuildAccelerationStructuresKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    payload.write_pod(infoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        const std::uint32_t rvk_n = static_cast<std::uint32_t>(pInfos[rvk_i].geometryCount);
        for (std::uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[rvk_i][rvk_j]);
        }
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBuildAccelerationStructuresKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBindShadersEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stageCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(stageCount); ++rvk_i) {
        payload.write_pod(pStages[rvk_i]);
    }
    const std::uint8_t pShaders_present = pShaders != nullptr ? 1 : 0;
    payload.write_pod(pShaders_present);
    if (pShaders_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(stageCount); ++rvk_i) {
            serialize_VkShaderEXT(payload, pShaders[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindShadersEXT, payload.bytes());
}

void rvk_vkCmdBindVertexBuffers2EXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    const std::uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pStrides[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT, payload.bytes());
}

void rvk_vkCmdBuildAccelerationStructuresIndirectKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        payload.write_pod(pIndirectDeviceAddresses[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        payload.write_pod(pIndirectStrides[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        const std::uint32_t rvk_n = static_cast<std::uint32_t>(pInfos[rvk_i].geometryCount);
        for (std::uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            payload.write_pod(ppMaxPrimitiveCounts[rvk_i][rvk_j]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR, payload.bytes());
}

void rvk_vkCmdBuildAccelerationStructuresKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(infoCount); ++rvk_i) {
        const std::uint32_t rvk_n = static_cast<std::uint32_t>(pInfos[rvk_i].geometryCount);
        for (std::uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[rvk_i][rvk_j]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR, payload.bytes());
}

void rvk_vkCmdCopyAccelerationStructureKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR, payload.bytes());
}

void rvk_vkCmdCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR, payload.bytes());
}

void rvk_vkCmdCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR, payload.bytes());
}

void rvk_vkCmdDrawMeshTasksEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT, payload.bytes());
}

void rvk_vkCmdDrawMeshTasksIndirectCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT, payload.bytes());
}

void rvk_vkCmdDrawMeshTasksIndirectEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT, payload.bytes());
}

void rvk_vkCmdPushDescriptorSetKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(set);
    payload.write_pod(descriptorWriteCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(descriptorWriteCount); ++rvk_i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR, payload.bytes());
}

void rvk_vkCmdSetAlphaToCoverageEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToCoverageEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT, payload.bytes());
}

void rvk_vkCmdSetAlphaToOneEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToOneEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT, payload.bytes());
}

void rvk_vkCmdSetColorBlendAdvancedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkColorBlendAdvancedEXT(payload, pColorBlendAdvanced[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT, payload.bytes());
}

void rvk_vkCmdSetColorBlendEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorBlendEnables[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT, payload.bytes());
}

void rvk_vkCmdSetColorBlendEquationEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkColorBlendEquationEXT(payload, pColorBlendEquations[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT, payload.bytes());
}

void rvk_vkCmdSetColorWriteMaskEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorWriteMasks[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT, payload.bytes());
}

void rvk_vkCmdSetConservativeRasterizationModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(conservativeRasterizationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT, payload.bytes());
}

void rvk_vkCmdSetCoverageModulationModeNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV, payload.bytes());
}

void rvk_vkCmdSetCoverageModulationTableEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV, payload.bytes());
}

void rvk_vkCmdSetCoverageModulationTableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(coverageModulationTableCount); ++rvk_i) {
        payload.write_pod(pCoverageModulationTable[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV, payload.bytes());
}

void rvk_vkCmdSetCoverageReductionModeNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageReductionMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV, payload.bytes());
}

void rvk_vkCmdSetCoverageToColorEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV, payload.bytes());
}

void rvk_vkCmdSetCoverageToColorLocationNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorLocation);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV, payload.bytes());
}

void rvk_vkCmdSetCullModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCullModeEXT, payload.bytes());
}

void rvk_vkCmdSetDepthBoundsTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT, payload.bytes());
}

void rvk_vkCmdSetDepthClampEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT, payload.bytes());
}

void rvk_vkCmdSetDepthClampRangeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampMode);
    serialize_VkDepthClampRangeEXT(payload, *pDepthClampRange);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT, payload.bytes());
}

void rvk_vkCmdSetDepthClipEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClipEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT, payload.bytes());
}

void rvk_vkCmdSetDepthClipNegativeOneToOneEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(negativeOneToOne);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT, payload.bytes());
}

void rvk_vkCmdSetDepthCompareOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT, payload.bytes());
}

void rvk_vkCmdSetDepthTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT, payload.bytes());
}

void rvk_vkCmdSetDepthWriteEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT, payload.bytes());
}

void rvk_vkCmdSetExtraPrimitiveOverestimationSizeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(extraPrimitiveOverestimationSize);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT, payload.bytes());
}

void rvk_vkCmdSetFragmentShadingRateKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkExtent2D(payload, *pFragmentSize);
    for (std::uint32_t rvk_i = 0; rvk_i < 2u; ++rvk_i) {
        payload.write_pod(combinerOps[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR, payload.bytes());
}

void rvk_vkCmdSetFrontFaceEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFaceEXT, payload.bytes());
}

void rvk_vkCmdSetLineRasterizationModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineRasterizationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT, payload.bytes());
}

void rvk_vkCmdSetLineStippleEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineStippleFactor);
    payload.write_pod(lineStipplePattern);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEXT, payload.bytes());
}

void rvk_vkCmdSetLineStippleEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stippledLineEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT, payload.bytes());
}

void rvk_vkCmdSetLogicOpEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOpEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT, payload.bytes());
}

void rvk_vkCmdSetPolygonModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(polygonMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPolygonModeEXT, payload.bytes());
}

void rvk_vkCmdSetPrimitiveTopologyEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT, payload.bytes());
}

void rvk_vkCmdSetProvokingVertexModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(provokingVertexMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT, payload.bytes());
}

void rvk_vkCmdSetRasterizationSamplesEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationSamples);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT, payload.bytes());
}

void rvk_vkCmdSetRasterizationStreamEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationStream);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT, payload.bytes());
}

void rvk_vkCmdSetRayTracingPipelineStackSizeKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStackSize);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR, payload.bytes());
}

void rvk_vkCmdSetRepresentativeFragmentTestEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(representativeFragmentTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV, payload.bytes());
}

void rvk_vkCmdSetSampleLocationsEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(sampleLocationsEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT, payload.bytes());
}

void rvk_vkCmdSetSampleMaskEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(samples);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>((samples + 31) / 32); ++rvk_i) {
        payload.write_pod(pSampleMask[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetSampleMaskEXT, payload.bytes());
}

void rvk_vkCmdSetScissorWithCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT, payload.bytes());
}

void rvk_vkCmdSetShadingRateImageEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(shadingRateImageEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV, payload.bytes());
}

void rvk_vkCmdSetStencilOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOpEXT, payload.bytes());
}

void rvk_vkCmdSetStencilTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT, payload.bytes());
}

void rvk_vkCmdSetTessellationDomainOriginEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(domainOrigin);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT, payload.bytes());
}

void rvk_vkCmdSetViewportSwizzleNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewportSwizzleNV(payload, pViewportSwizzles[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV, payload.bytes());
}

void rvk_vkCmdSetViewportWScalingEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportWScalingEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV, payload.bytes());
}

void rvk_vkCmdSetViewportWithCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT, payload.bytes());
}

void rvk_vkCmdTraceRaysIndirectKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(indirectDeviceAddress);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR, payload.bytes());
}

void rvk_vkCmdTraceRaysKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(width);
    payload.write_pod(height);
    payload.write_pod(depth);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysKHR, payload.bytes());
}

void rvk_vkCmdWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(accelerationStructureCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(accelerationStructureCount); ++rvk_i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[rvk_i]);
    }
    payload.write_pod(queryType);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR, payload.bytes());
}

VkResult rvk_vkCopyAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToImageEXT(::av::ClientTransport& t, VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToImageInfo(payload, *pCopyImageToImageInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToImageEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToMemoryEXT(::av::ClientTransport& t, VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToMemoryInfo(payload, *pCopyImageToMemoryInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToMemoryEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToImageEXT(::av::ClientTransport& t, VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyMemoryToImageInfo(payload, *pCopyMemoryToImageInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToImageEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateAccelerationStructureKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pAccelerationStructure)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDeferredOperationKHR(::av::ClientTransport& t, VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDeferredOperationKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDeferredOperation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRayTracingPipelinesKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkRayTracingPipelineCreateInfoKHR(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateShadersEXT(::av::ClientTransport& t, VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(createInfoCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkShaderCreateInfoEXT(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShadersEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pShaders[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkDeferredOperationJoinKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkDeferredOperationJoinKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureKHR(payload, accelerationStructure);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR, payload.bytes());
}

void rvk_vkDestroyDeferredOperationKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDeferredOperationKHR, payload.bytes());
}

void rvk_vkDestroyShaderEXT(::av::ClientTransport& t, VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderEXT(payload, shader);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyShaderEXT, payload.bytes());
}

void rvk_vkGetAccelerationStructureBuildSizesKHR(::av::ClientTransport& t, VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(buildType);
    serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, *pBuildInfo);
    const std::uint8_t pMaxPrimitiveCounts_present = pMaxPrimitiveCounts != nullptr ? 1 : 0;
    payload.write_pod(pMaxPrimitiveCounts_present);
    if (pMaxPrimitiveCounts_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(pBuildInfo->geometryCount); ++rvk_i) {
            payload.write_pod(pMaxPrimitiveCounts[rvk_i]);
        }
    }
    payload.write_pod(pSizeInfo->sType);
    serialize_pnext_chain(payload, pSizeInfo->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkAccelerationStructureBuildSizesInfoKHR(r, pSizeInfo, arena_out)) {
        return;
    }
}

VkDeviceAddress rvk_vkGetAccelerationStructureDeviceAddressKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureDeviceAddressInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureDeviceAddressKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkDeviceAddress>(result_code);
}

uint32_t rvk_vkGetDeferredOperationMaxConcurrencyKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationMaxConcurrencyKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<uint32_t>(result_code);
}

VkResult rvk_vkGetDeferredOperationResultKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationResultKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceAccelerationStructureCompatibilityKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureVersionInfoKHR(payload, *pVersionInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceAccelerationStructureCompatibilityKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pCompatibility)) {
        return;
    }
}

void rvk_vkGetImageSubresourceLayout2EXT(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkSubresourceLayout2(r, pLayout, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceFragmentShadingRatesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pFragmentShadingRates != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pFragmentShadingRateCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFragmentShadingRatesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFragmentShadingRateCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_arr;
    if (array_present) {
        for (std::uint32_t k = 0; k < *pFragmentShadingRateCount; ++k) {
            if (!deserialize_VkPhysicalDeviceFragmentShadingRateKHR(r, &pFragmentShadingRates[k], arena_arr)) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<std::uint64_t>(dataSize);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<std::size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingShaderGroupHandlesKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<std::uint64_t>(dataSize);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupHandlesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<std::size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkDeviceSize rvk_vkGetRayTracingShaderGroupStackSizeKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(group);
    payload.write_pod(groupShader);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupStackSizeKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkDeviceSize>(result_code);
}

VkResult rvk_vkTransitionImageLayoutEXT(::av::ClientTransport& t, VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(transitionCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(transitionCount); ++rvk_i) {
        serialize_VkHostImageLayoutTransitionInfo(payload, pTransitions[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkTransitionImageLayoutEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(accelerationStructureCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(accelerationStructureCount); ++rvk_i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[rvk_i]);
    }
    payload.write_pod(queryType);
    payload.write_pod<std::uint64_t>(dataSize);
    payload.write_pod<std::uint64_t>(stride);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<std::size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetCalibratedTimestampsEXT(::av::ClientTransport& t, VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(timestampCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(timestampCount); ++rvk_i) {
        serialize_VkCalibratedTimestampInfoKHR(payload, pTimestampInfos[rvk_i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t rvk_k = 0; rvk_k < static_cast<std::uint32_t>(timestampCount); ++rvk_k) {
        if (!r.read_pod(pTimestamps[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    if (!r.read_pod(*pMaxDeviation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pTimeDomains != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pTimeDomainCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pTimeDomainCount)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        for (std::uint32_t k = 0; k < *pTimeDomainCount; ++k) {
            if (!r.read_pod(pTimeDomains[k])) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetRenderingAttachmentLocationsKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingAttachmentLocationInfo(payload, *pLocationInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR, payload.bytes());
}

void rvk_vkCmdSetRenderingInputAttachmentIndicesKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInputAttachmentIndexInfo(payload, *pInputAttachmentIndexInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR, payload.bytes());
}

void rvk_vkCmdBindIndexBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(size);
    payload.write_pod(indexType);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR, payload.bytes());
}

VkResult rvk_vkCreatePipelineBinariesKHR(::av::ClientTransport& t, VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    payload.write_pod(pBinaries->sType);
    serialize_pnext_chain(payload, pBinaries->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineBinariesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPipelineBinaryHandlesInfoKHR(r, pBinaries, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineBinaryKHR(::av::ClientTransport& t, VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryKHR(payload, pipelineBinary);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR, payload.bytes());
}

void rvk_vkGetDeviceImageSubresourceLayoutKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageSubresourceInfo(payload, *pInfo);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkSubresourceLayout2(r, pLayout, arena_out)) {
        return;
    }
}

void rvk_vkGetImageSubresourceLayout2KHR(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkSubresourceLayout2(r, pLayout, arena_out)) {
        return;
    }
}

VkResult rvk_vkGetPipelineKeyKHR(::av::ClientTransport& t, VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCreateInfoKHR(payload, *pPipelineCreateInfo);
    payload.write_pod(pPipelineKey->sType);
    serialize_pnext_chain(payload, pPipelineKey->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineKeyKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkPipelineBinaryKeyKHR(r, pPipelineKey, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderingAreaGranularityKHR(::av::ClientTransport& t, VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderingAreaInfo(payload, *pRenderingAreaInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderingAreaGranularityKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkExtent2D(r, pGranularity, arena_out)) {
        return;
    }
}

VkResult rvk_vkReleaseCapturedPipelineDataKHR(::av::ClientTransport& t, VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkReleaseCapturedPipelineDataInfoKHR(payload, *pInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkReleaseCapturedPipelineDataKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQueryIndexedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    payload.write_pod(index);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT, payload.bytes());
}

void rvk_vkCmdBeginTransformFeedbackEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(counterBufferCount); ++rvk_i) {
        serialize_VkBuffer(payload, pCounterBuffers[rvk_i]);
    }
    const std::uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(counterBufferCount); ++rvk_i) {
            payload.write_pod(pCounterBufferOffsets[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT, payload.bytes());
}

void rvk_vkCmdBindTransformFeedbackBuffersEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT, payload.bytes());
}

void rvk_vkCmdDrawIndirectByteCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    serialize_VkBuffer(payload, counterBuffer);
    payload.write_pod(counterBufferOffset);
    payload.write_pod(counterOffset);
    payload.write_pod(vertexStride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT, payload.bytes());
}

void rvk_vkCmdEndQueryIndexedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(index);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT, payload.bytes());
}

void rvk_vkCmdEndTransformFeedbackEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(counterBufferCount); ++rvk_i) {
        serialize_VkBuffer(payload, pCounterBuffers[rvk_i]);
    }
    const std::uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (std::uint32_t rvk_i = 0; rvk_i < static_cast<std::uint32_t>(counterBufferCount); ++rvk_i) {
            payload.write_pod(pCounterBufferOffsets[rvk_i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT, payload.bytes());
}

void rvk_vkSetDeviceMemoryPriorityEXT(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, float priority) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(priority);
    (void)t.send_async(::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT, payload.bytes());
}

VkResult rvk_vkGetFenceFdKHR(::av::ClientTransport& t, VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFenceGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceFdKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdKHR(::av::ClientTransport& t, VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdPropertiesKHR(::av::ClientTransport& t, VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(handleType);
    payload.write_pod(fd);
    payload.write_pod(pMemoryFdProperties->sType);
    serialize_pnext_chain(payload, pMemoryFdProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireArena arena_out;
    if (!deserialize_VkMemoryFdPropertiesKHR(r, pMemoryFdProperties, arena_out)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetSemaphoreFdKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreFdKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportFenceFdKHR(::av::ClientTransport& t, VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImportFenceFdInfoKHR(payload, *pImportFenceFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkImportFenceFdKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportSemaphoreFdKHR(::av::ClientTransport& t, VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImportSemaphoreFdInfoKHR(payload, *pImportSemaphoreFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkImportSemaphoreFdKHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkUnmapMemory2KHR(::av::ClientTransport& t, VkDevice device, const VkMemoryUnmapInfo* pMemoryUnmapInfo) {
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryUnmapInfo(payload, *pMemoryUnmapInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkUnmapMemory2KHR, payload.bytes(), &reply) != ::av::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

}  // namespace av::vkproto
