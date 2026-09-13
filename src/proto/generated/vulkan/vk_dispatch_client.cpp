// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_dispatch_client.h"

#include "rvk/instr/log.hpp"
#include "proto/wire/wire_writer.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

void rvk_vkDestroyInstance(::rvk::protocol::Transport& t, VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyInstance(instance={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(instance));
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyInstance, payload.bytes());
    RVK_TRACE("client vkDestroyInstance -> (async, no reply expected)");
}

void rvk_vkDestroyDevice(::rvk::protocol::Transport& t, VkDevice device, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDevice(device={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDevice, payload.bytes());
    RVK_TRACE("client vkDestroyDevice -> (async, no reply expected)");
}

void rvk_vkGetDeviceQueue(::rvk::protocol::Transport& t, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, std::uint64_t* pQueue) {
    RVK_TRACE("client vkGetDeviceQueue(device={:#x}, queueFamilyIndex={}, queueIndex={}, pQueue=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(queueFamilyIndex), static_cast<long long>(queueIndex));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(queueFamilyIndex);
    payload.write_pod(queueIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceQueue -> result_code={}", result_code);
}

VkResult rvk_vkEnumeratePhysicalDevices(::rvk::protocol::Transport& t, VkInstance instance, uint32_t* pPhysicalDeviceCount, std::uint64_t* pPhysicalDevices) {
    RVK_TRACE("client vkEnumeratePhysicalDevices(instance={:#x}, pPhysicalDeviceCount=ptr, pPhysicalDevices=ptr)",::rvk::instr::trace_handle(instance));
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDevices != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDevices, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkEnumeratePhysicalDevices -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceProperties2(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceProperties2 -> result_code={}", result_code);
}

VkResult rvk_vkCreateInstance(::rvk::protocol::Transport& t, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, std::uint64_t* pInstance) {
    RVK_TRACE("client vkCreateInstance(pCreateInfo=ptr, pAllocator=ptr, pInstance=ptr)");
    ::av::WireWriter payload;
    serialize_VkInstanceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateInstance, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateInstance -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDevice(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, std::uint64_t* pDevice) {
    RVK_TRACE("client vkCreateDevice(physicalDevice={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDevice=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkDeviceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDevice, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDevice -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceProperties(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceProperties -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceFeatures(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    RVK_TRACE("client vkGetPhysicalDeviceFeatures(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFeatures -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceFormatProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties -> result_code={}", result_code);
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties(physicalDevice={:#x}, format={}, type={}, tiling={}, usage={}, flags={}, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format), static_cast<long long>(type), static_cast<long long>(tiling), static_cast<long long>(usage), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(tiling);
    payload.write_pod(usage);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice={:#x}, format={}, type={}, samples={}, usage={}, tiling={}, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format), static_cast<long long>(type), static_cast<long long>(samples), static_cast<long long>(usage), static_cast<long long>(tiling));
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
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties -> result_code={}", result_code);
}

VkResult rvk_vkEnumerateDeviceExtensionProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    RVK_TRACE("client vkEnumerateDeviceExtensionProperties(physicalDevice={:#x}, pLayerName=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
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
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceExtensionProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkEnumerateDeviceExtensionProperties -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkEnumerateDeviceLayerProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    RVK_TRACE("client vkEnumerateDeviceLayerProperties(physicalDevice={:#x}, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceLayerProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkEnumerateDeviceLayerProperties -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    RVK_TRACE("client vkGetPhysicalDeviceFeatures2(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFeatures2 -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceFormatProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties2(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties2 -> result_code={}", result_code);
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties2(physicalDevice={:#x}, pImageFormatInfo=ptr, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties2 -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties2(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties2 -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties2 -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice={:#x}, pFormatInfo=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties2 -> result_code={}", result_code);
}

VkResult rvk_vkDeviceWaitIdle(::rvk::protocol::Transport& t, VkDevice device) {
    RVK_TRACE("client vkDeviceWaitIdle(device={:#x})",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkDeviceWaitIdle, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkDeviceWaitIdle -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueWaitIdle(::rvk::protocol::Transport& t, VkQueue queue) {
    RVK_TRACE("client vkQueueWaitIdle(queue={:#x})",::rvk::instr::trace_handle(queue));
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueWaitIdle, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkQueueWaitIdle -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateMemory(::rvk::protocol::Transport& t, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    RVK_TRACE("client vkAllocateMemory(device={:#x}, pAllocateInfo=ptr, pAllocator=ptr, pMemory=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryAllocateInfo(payload, *pAllocateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateMemory, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkAllocateMemory -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkFreeMemory(::rvk::protocol::Transport& t, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkFreeMemory(device={:#x}, memory={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkFreeMemory, payload.bytes());
    RVK_TRACE("client vkFreeMemory -> (async, no reply expected)");
}

VkResult rvk_vkBindBufferMemory(::rvk::protocol::Transport& t, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    RVK_TRACE("client vkBindBufferMemory(device={:#x}, buffer={:#x}, memory={:#x}, memoryOffset={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer), ::rvk::instr::trace_handle(memory), static_cast<long long>(memoryOffset));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindBufferMemory -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory(::rvk::protocol::Transport& t, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    RVK_TRACE("client vkBindImageMemory(device={:#x}, image={:#x}, memory={:#x}, memoryOffset={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image), ::rvk::instr::trace_handle(memory), static_cast<long long>(memoryOffset));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindImageMemory -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    RVK_TRACE("client vkGetBufferMemoryRequirements(device={:#x}, buffer={:#x}, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetBufferMemoryRequirements -> result_code={}", result_code);
}

void rvk_vkGetImageMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    RVK_TRACE("client vkGetImageMemoryRequirements(device={:#x}, image={:#x}, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageMemoryRequirements -> result_code={}", result_code);
}

VkResult rvk_vkCreateBuffer(::rvk::protocol::Transport& t, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    RVK_TRACE("client vkCreateBuffer(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pBuffer=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBuffer, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateBuffer -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBuffer(::rvk::protocol::Transport& t, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyBuffer(device={:#x}, buffer={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyBuffer, payload.bytes());
    RVK_TRACE("client vkDestroyBuffer -> (async, no reply expected)");
}

VkResult rvk_vkCreateImage(::rvk::protocol::Transport& t, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    RVK_TRACE("client vkCreateImage(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pImage=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImage, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateImage -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImage(::rvk::protocol::Transport& t, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyImage(device={:#x}, image={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyImage, payload.bytes());
    RVK_TRACE("client vkDestroyImage -> (async, no reply expected)");
}

VkResult rvk_vkCreateImageView(::rvk::protocol::Transport& t, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    RVK_TRACE("client vkCreateImageView(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pView=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImageView, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateImageView -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImageView(::rvk::protocol::Transport& t, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyImageView(device={:#x}, imageView={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(imageView));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageView(payload, imageView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyImageView, payload.bytes());
    RVK_TRACE("client vkDestroyImageView -> (async, no reply expected)");
}

VkResult rvk_vkCreateSampler(::rvk::protocol::Transport& t, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    RVK_TRACE("client vkCreateSampler(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSampler=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSampler, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateSampler -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySampler(::rvk::protocol::Transport& t, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroySampler(device={:#x}, sampler={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(sampler));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSampler(payload, sampler);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySampler, payload.bytes());
    RVK_TRACE("client vkDestroySampler -> (async, no reply expected)");
}

VkResult rvk_vkCreateSemaphore(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    RVK_TRACE("client vkCreateSemaphore(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSemaphore=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSemaphore, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateSemaphore -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySemaphore(::rvk::protocol::Transport& t, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroySemaphore(device={:#x}, semaphore={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySemaphore, payload.bytes());
    RVK_TRACE("client vkDestroySemaphore -> (async, no reply expected)");
}

VkResult rvk_vkCreateFence(::rvk::protocol::Transport& t, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    RVK_TRACE("client vkCreateFence(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pFence=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFenceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFence, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateFence -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFence(::rvk::protocol::Transport& t, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyFence(device={:#x}, fence={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyFence, payload.bytes());
    RVK_TRACE("client vkDestroyFence -> (async, no reply expected)");
}

VkResult rvk_vkResetFences(::rvk::protocol::Transport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    RVK_TRACE("client vkResetFences(device={:#x}, fenceCount={}, pFences=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(fenceCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(fenceCount); ++__i) {
        serialize_VkFence(payload, pFences[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkResetFences, payload.bytes());
    RVK_TRACE("client vkResetFences -> (async, no reply expected)");
    return VK_SUCCESS;
}

VkResult rvk_vkWaitForFences(::rvk::protocol::Transport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    RVK_TRACE("client vkWaitForFences(device={:#x}, fenceCount={}, pFences=ptr, waitAll={}, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(fenceCount), static_cast<long long>(waitAll), static_cast<long long>(timeout));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(fenceCount); ++__i) {
        serialize_VkFence(payload, pFences[__i]);
    }
    payload.write_pod(waitAll);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitForFences, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkWaitForFences -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetFenceStatus(::rvk::protocol::Transport& t, VkDevice device, VkFence fence) {
    RVK_TRACE("client vkGetFenceStatus(device={:#x}, fence={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceStatus, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetFenceStatus -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateCommandPool(::rvk::protocol::Transport& t, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    RVK_TRACE("client vkCreateCommandPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pCommandPool=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateCommandPool, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateCommandPool -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyCommandPool(::rvk::protocol::Transport& t, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyCommandPool(device={:#x}, commandPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyCommandPool, payload.bytes());
    RVK_TRACE("client vkDestroyCommandPool -> (async, no reply expected)");
}

VkResult rvk_vkResetCommandPool(::rvk::protocol::Transport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    RVK_TRACE("client vkResetCommandPool(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkResetCommandPool, payload.bytes());
    RVK_TRACE("client vkResetCommandPool -> (async, no reply expected)");
    return VK_SUCCESS;
}

VkResult rvk_vkBeginCommandBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    RVK_TRACE("client vkBeginCommandBuffer(commandBuffer={:#x}, pBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCommandBufferBeginInfo(payload, *pBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkBeginCommandBuffer, payload.bytes());
    RVK_TRACE("client vkBeginCommandBuffer -> (async, no reply expected)");
    return VK_SUCCESS;
}

VkResult rvk_vkEndCommandBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer) {
    RVK_TRACE("client vkEndCommandBuffer(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkEndCommandBuffer, payload.bytes());
    RVK_TRACE("client vkEndCommandBuffer -> (async, no reply expected)");
    return VK_SUCCESS;
}

VkResult rvk_vkResetCommandBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    RVK_TRACE("client vkResetCommandBuffer(commandBuffer={:#x}, flags={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkResetCommandBuffer, payload.bytes());
    RVK_TRACE("client vkResetCommandBuffer -> (async, no reply expected)");
    return VK_SUCCESS;
}

void rvk_vkDestroyShaderModule(::rvk::protocol::Transport& t, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyShaderModule(device={:#x}, shaderModule={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(shaderModule));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderModule(payload, shaderModule);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyShaderModule, payload.bytes());
    RVK_TRACE("client vkDestroyShaderModule -> (async, no reply expected)");
}

VkResult rvk_vkCreateShaderModule(::rvk::protocol::Transport& t, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    RVK_TRACE("client vkCreateShaderModule(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pShaderModule=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderModuleCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShaderModule, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateShaderModule -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePipelineLayout(::rvk::protocol::Transport& t, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    RVK_TRACE("client vkCreatePipelineLayout(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPipelineLayout=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineLayout, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreatePipelineLayout -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineLayout(::rvk::protocol::Transport& t, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyPipelineLayout(device={:#x}, pipelineLayout={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineLayout));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayout(payload, pipelineLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineLayout, payload.bytes());
    RVK_TRACE("client vkDestroyPipelineLayout -> (async, no reply expected)");
}

void rvk_vkDestroyPipeline(::rvk::protocol::Transport& t, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyPipeline(device={:#x}, pipeline={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipeline, payload.bytes());
    RVK_TRACE("client vkDestroyPipeline -> (async, no reply expected)");
}

VkResult rvk_vkCreatePipelineCache(::rvk::protocol::Transport& t, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    RVK_TRACE("client vkCreatePipelineCache(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPipelineCache=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCacheCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineCache, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreatePipelineCache -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineCache(::rvk::protocol::Transport& t, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyPipelineCache(device={:#x}, pipelineCache={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineCache, payload.bytes());
    RVK_TRACE("client vkDestroyPipelineCache -> (async, no reply expected)");
}

VkResult rvk_vkCreateRenderPass(::rvk::protocol::Transport& t, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    RVK_TRACE("client vkCreateRenderPass(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateRenderPass -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyRenderPass(::rvk::protocol::Transport& t, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyRenderPass(device={:#x}, renderPass={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(renderPass));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyRenderPass, payload.bytes());
    RVK_TRACE("client vkDestroyRenderPass -> (async, no reply expected)");
}

VkResult rvk_vkCreateFramebuffer(::rvk::protocol::Transport& t, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    RVK_TRACE("client vkCreateFramebuffer(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pFramebuffer=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFramebufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFramebuffer, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateFramebuffer -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFramebuffer(::rvk::protocol::Transport& t, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyFramebuffer(device={:#x}, framebuffer={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(framebuffer));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFramebuffer(payload, framebuffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyFramebuffer, payload.bytes());
    RVK_TRACE("client vkDestroyFramebuffer -> (async, no reply expected)");
}

VkResult rvk_vkCreateDescriptorSetLayout(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    RVK_TRACE("client vkCreateDescriptorSetLayout(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSetLayout=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorSetLayout, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDescriptorSetLayout -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorSetLayout(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDescriptorSetLayout(device={:#x}, descriptorSetLayout={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorSetLayout));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayout(payload, descriptorSetLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorSetLayout, payload.bytes());
    RVK_TRACE("client vkDestroyDescriptorSetLayout -> (async, no reply expected)");
}

VkResult rvk_vkCreateDescriptorPool(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    RVK_TRACE("client vkCreateDescriptorPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorPool=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorPool, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDescriptorPool -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorPool(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDescriptorPool(device={:#x}, descriptorPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorPool, payload.bytes());
    RVK_TRACE("client vkDestroyDescriptorPool -> (async, no reply expected)");
}

void rvk_vkFreeCommandBuffers(::rvk::protocol::Transport& t, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    RVK_TRACE("client vkFreeCommandBuffers(device={:#x}, commandPool={:#x}, commandBufferCount={}, pCommandBuffers=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(commandBufferCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(commandBufferCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(commandBufferCount); ++__i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkFreeCommandBuffers, payload.bytes());
    RVK_TRACE("client vkFreeCommandBuffers -> (async, no reply expected)");
}

VkResult rvk_vkQueueSubmit(::rvk::protocol::Transport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    RVK_TRACE("client vkQueueSubmit(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
        serialize_VkSubmitInfo(payload, pSubmits[__i]);
    }
    serialize_VkFence(payload, fence);
    (void)t.send_async(::av::vkproto::kCmd_vkQueueSubmit, payload.bytes());
    RVK_TRACE("client vkQueueSubmit -> (async, no reply expected)");
    return VK_SUCCESS;
}

void rvk_vkCmdBindPipeline(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    RVK_TRACE("client vkCmdBindPipeline(commandBuffer={:#x}, pipelineBindPoint={}, pipeline={:#x})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(pipeline));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipeline(payload, pipeline);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindPipeline, payload.bytes());
    RVK_TRACE("client vkCmdBindPipeline -> (async, no reply expected)");
}

void rvk_vkCmdSetViewport(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    RVK_TRACE("client vkCmdSetViewport(commandBuffer={:#x}, firstViewport={}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstViewport), static_cast<long long>(viewportCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
        serialize_VkViewport(payload, pViewports[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewport, payload.bytes());
    RVK_TRACE("client vkCmdSetViewport -> (async, no reply expected)");
}

void rvk_vkCmdSetScissor(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    RVK_TRACE("client vkCmdSetScissor(commandBuffer={:#x}, firstScissor={}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstScissor), static_cast<long long>(scissorCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstScissor);
    payload.write_pod(scissorCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
        serialize_VkRect2D(payload, pScissors[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissor, payload.bytes());
    RVK_TRACE("client vkCmdSetScissor -> (async, no reply expected)");
}

void rvk_vkCmdBindVertexBuffers(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    RVK_TRACE("client vkCmdBindVertexBuffers(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        serialize_VkBuffer(payload, pBuffers[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        payload.write_pod(pOffsets[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers, payload.bytes());
    RVK_TRACE("client vkCmdBindVertexBuffers -> (async, no reply expected)");
}

void rvk_vkCmdBindIndexBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    RVK_TRACE("client vkCmdBindIndexBuffer(commandBuffer={:#x}, buffer={:#x}, offset={}, indexType={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(indexType));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(indexType);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer, payload.bytes());
    RVK_TRACE("client vkCmdBindIndexBuffer -> (async, no reply expected)");
}

void rvk_vkCmdBindDescriptorSets(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    RVK_TRACE("client vkCmdBindDescriptorSets(commandBuffer={:#x}, pipelineBindPoint={}, layout={:#x}, firstSet={}, descriptorSetCount={}, pDescriptorSets=ptr, dynamicOffsetCount={}, pDynamicOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(layout), static_cast<long long>(firstSet), static_cast<long long>(descriptorSetCount), static_cast<long long>(dynamicOffsetCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(firstSet);
    payload.write_pod(descriptorSetCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorSetCount); ++__i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[__i]);
    }
    payload.write_pod(dynamicOffsetCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(dynamicOffsetCount); ++__i) {
        payload.write_pod(pDynamicOffsets[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorSets, payload.bytes());
    RVK_TRACE("client vkCmdBindDescriptorSets -> (async, no reply expected)");
}

void rvk_vkCmdDraw(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    RVK_TRACE("client vkCmdDraw(commandBuffer={:#x}, vertexCount={}, instanceCount={}, firstVertex={}, firstInstance={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(vertexCount), static_cast<long long>(instanceCount), static_cast<long long>(firstVertex), static_cast<long long>(firstInstance));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstVertex);
    payload.write_pod(firstInstance);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDraw, payload.bytes());
    RVK_TRACE("client vkCmdDraw -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndexed(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    RVK_TRACE("client vkCmdDrawIndexed(commandBuffer={:#x}, indexCount={}, instanceCount={}, firstIndex={}, vertexOffset={}, firstInstance={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(indexCount), static_cast<long long>(instanceCount), static_cast<long long>(firstIndex), static_cast<long long>(vertexOffset), static_cast<long long>(firstInstance));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(indexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstIndex);
    payload.write_pod(vertexOffset);
    payload.write_pod(firstInstance);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexed, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndexed -> (async, no reply expected)");
}

void rvk_vkCmdPipelineBarrier(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    RVK_TRACE("client vkCmdPipelineBarrier(commandBuffer={:#x}, srcStageMask={}, dstStageMask={}, dependencyFlags={}, memoryBarrierCount={}, pMemoryBarriers=ptr, bufferMemoryBarrierCount={}, pBufferMemoryBarriers=ptr, imageMemoryBarrierCount={}, pImageMemoryBarriers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(srcStageMask), static_cast<long long>(dstStageMask), static_cast<long long>(dependencyFlags), static_cast<long long>(memoryBarrierCount), static_cast<long long>(bufferMemoryBarrierCount), static_cast<long long>(imageMemoryBarrierCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(dependencyFlags);
    payload.write_pod(memoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryBarrierCount); ++__i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[__i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++__i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[__i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++__i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier, payload.bytes());
    RVK_TRACE("client vkCmdPipelineBarrier -> (async, no reply expected)");
}

void rvk_vkCmdCopyBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    RVK_TRACE("client vkCmdCopyBuffer(commandBuffer={:#x}, srcBuffer={:#x}, dstBuffer={:#x}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(regionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkBufferCopy(payload, pRegions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer, payload.bytes());
    RVK_TRACE("client vkCmdCopyBuffer -> (async, no reply expected)");
}

void rvk_vkCmdBeginRenderPass(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    RVK_TRACE("client vkCmdBeginRenderPass(commandBuffer={:#x}, pRenderPassBegin=ptr, contents={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(contents));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    payload.write_pod(contents);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass, payload.bytes());
    RVK_TRACE("client vkCmdBeginRenderPass -> (async, no reply expected)");
}

void rvk_vkCmdEndRenderPass(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer) {
    RVK_TRACE("client vkCmdEndRenderPass(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass, payload.bytes());
    RVK_TRACE("client vkCmdEndRenderPass -> (async, no reply expected)");
}

void rvk_vkCmdNextSubpass(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    RVK_TRACE("client vkCmdNextSubpass(commandBuffer={:#x}, contents={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(contents));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(contents);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass, payload.bytes());
    RVK_TRACE("client vkCmdNextSubpass -> (async, no reply expected)");
}

void rvk_vkCmdPushConstants(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    RVK_TRACE("client vkCmdPushConstants(commandBuffer={:#x}, layout={:#x}, stageFlags={}, offset={}, size={}, pValues=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(layout), static_cast<long long>(stageFlags), static_cast<long long>(offset), static_cast<long long>(size));
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
    RVK_TRACE("client vkCmdPushConstants -> (async, no reply expected)");
}

VkResult rvk_vkCreateGraphicsPipelines(::rvk::protocol::Transport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    RVK_TRACE("client vkCreateGraphicsPipelines(device={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
        serialize_VkGraphicsPipelineCreateInfo(payload, pCreateInfos[__i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateGraphicsPipelines, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(createInfoCount); ++__k) {
        if (!r.read_pod(pPipelines[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkCreateGraphicsPipelines -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkUpdateDescriptorSets(::rvk::protocol::Transport& t, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    RVK_TRACE("client vkUpdateDescriptorSets(device={:#x}, descriptorWriteCount={}, pDescriptorWrites=ptr, descriptorCopyCount={}, pDescriptorCopies=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(descriptorWriteCount), static_cast<long long>(descriptorCopyCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(descriptorWriteCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorWriteCount); ++__i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[__i]);
    }
    payload.write_pod(descriptorCopyCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorCopyCount); ++__i) {
        serialize_VkCopyDescriptorSet(payload, pDescriptorCopies[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSets, payload.bytes());
    RVK_TRACE("client vkUpdateDescriptorSets -> (async, no reply expected)");
}

VkResult rvk_vkAllocateCommandBuffers(::rvk::protocol::Transport& t, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, std::uint64_t* pCommandBuffers) {
    RVK_TRACE("client vkAllocateCommandBuffers(device={:#x}, pAllocateInfo=ptr, pCommandBuffers=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandBufferAllocateInfo(payload, *pAllocateInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateCommandBuffers, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(pAllocateInfo->commandBufferCount); ++__k) {
        if (!r.read_pod(pCommandBuffers[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkAllocateCommandBuffers -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateDescriptorSets(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    RVK_TRACE("client vkAllocateDescriptorSets(device={:#x}, pAllocateInfo=ptr, pDescriptorSets=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetAllocateInfo(payload, *pAllocateInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateDescriptorSets, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(pAllocateInfo->descriptorSetCount); ++__k) {
        if (!r.read_pod(pDescriptorSets[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkAllocateDescriptorSets -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFreeDescriptorSets(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    RVK_TRACE("client vkFreeDescriptorSets(device={:#x}, descriptorPool={:#x}, descriptorSetCount={}, pDescriptorSets=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool), static_cast<long long>(descriptorSetCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(descriptorSetCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorSetCount); ++__i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkFreeDescriptorSets, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkFreeDescriptorSets -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateComputePipelines(::rvk::protocol::Transport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    RVK_TRACE("client vkCreateComputePipelines(device={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
        serialize_VkComputePipelineCreateInfo(payload, pCreateInfos[__i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateComputePipelines, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(createInfoCount); ++__k) {
        if (!r.read_pod(pPipelines[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkCreateComputePipelines -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetDescriptorPool(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    RVK_TRACE("client vkResetDescriptorPool(device={:#x}, descriptorPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkResetDescriptorPool, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkResetDescriptorPool -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateEvent(::rvk::protocol::Transport& t, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    RVK_TRACE("client vkCreateEvent(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pEvent=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEventCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateEvent, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateEvent -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyEvent(::rvk::protocol::Transport& t, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyEvent(device={:#x}, event={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyEvent, payload.bytes());
    RVK_TRACE("client vkDestroyEvent -> (async, no reply expected)");
}

VkResult rvk_vkGetEventStatus(::rvk::protocol::Transport& t, VkDevice device, VkEvent event) {
    RVK_TRACE("client vkGetEventStatus(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetEventStatus, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetEventStatus -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSetEvent(::rvk::protocol::Transport& t, VkDevice device, VkEvent event) {
    RVK_TRACE("client vkSetEvent(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSetEvent, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkSetEvent -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetEvent(::rvk::protocol::Transport& t, VkDevice device, VkEvent event) {
    RVK_TRACE("client vkResetEvent(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkResetEvent, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkResetEvent -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetEvent(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    RVK_TRACE("client vkCmdSetEvent(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent, payload.bytes());
    RVK_TRACE("client vkCmdSetEvent -> (async, no reply expected)");
}

void rvk_vkCmdResetEvent(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    RVK_TRACE("client vkCmdResetEvent(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent, payload.bytes());
    RVK_TRACE("client vkCmdResetEvent -> (async, no reply expected)");
}

void rvk_vkCmdWaitEvents(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    RVK_TRACE("client vkCmdWaitEvents(commandBuffer={:#x}, eventCount={}, pEvents=ptr, srcStageMask={}, dstStageMask={}, memoryBarrierCount={}, pMemoryBarriers=ptr, bufferMemoryBarrierCount={}, pBufferMemoryBarriers=ptr, imageMemoryBarrierCount={}, pImageMemoryBarriers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount), static_cast<long long>(srcStageMask), static_cast<long long>(dstStageMask), static_cast<long long>(memoryBarrierCount), static_cast<long long>(bufferMemoryBarrierCount), static_cast<long long>(imageMemoryBarrierCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
        serialize_VkEvent(payload, pEvents[__i]);
    }
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(memoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryBarrierCount); ++__i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[__i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++__i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[__i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++__i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents, payload.bytes());
    RVK_TRACE("client vkCmdWaitEvents -> (async, no reply expected)");
}

void rvk_vkCmdCopyImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    RVK_TRACE("client vkCmdCopyImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkImageCopy(payload, pRegions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage, payload.bytes());
    RVK_TRACE("client vkCmdCopyImage -> (async, no reply expected)");
}

void rvk_vkCmdCopyBufferToImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    RVK_TRACE("client vkCmdCopyBufferToImage(commandBuffer={:#x}, srcBuffer={:#x}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcBuffer), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkBufferImageCopy(payload, pRegions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage, payload.bytes());
    RVK_TRACE("client vkCmdCopyBufferToImage -> (async, no reply expected)");
}

void rvk_vkCmdCopyImageToBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    RVK_TRACE("client vkCmdCopyImageToBuffer(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstBuffer={:#x}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(regionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkBufferImageCopy(payload, pRegions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer, payload.bytes());
    RVK_TRACE("client vkCmdCopyImageToBuffer -> (async, no reply expected)");
}

void rvk_vkCmdBlitImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    RVK_TRACE("client vkCmdBlitImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr, filter={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount), static_cast<long long>(filter));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkImageBlit(payload, pRegions[__i]);
    }
    payload.write_pod(filter);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage, payload.bytes());
    RVK_TRACE("client vkCmdBlitImage -> (async, no reply expected)");
}

void rvk_vkCmdClearColorImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    RVK_TRACE("client vkCmdClearColorImage(commandBuffer={:#x}, image={:#x}, imageLayout={}, pColor=ptr, rangeCount={}, pRanges=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(image), static_cast<long long>(imageLayout), static_cast<long long>(rangeCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearColorValue(payload, *pColor);
    payload.write_pod(rangeCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rangeCount); ++__i) {
        serialize_VkImageSubresourceRange(payload, pRanges[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearColorImage, payload.bytes());
    RVK_TRACE("client vkCmdClearColorImage -> (async, no reply expected)");
}

void rvk_vkCmdClearDepthStencilImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    RVK_TRACE("client vkCmdClearDepthStencilImage(commandBuffer={:#x}, image={:#x}, imageLayout={}, pDepthStencil=ptr, rangeCount={}, pRanges=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(image), static_cast<long long>(imageLayout), static_cast<long long>(rangeCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearDepthStencilValue(payload, *pDepthStencil);
    payload.write_pod(rangeCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rangeCount); ++__i) {
        serialize_VkImageSubresourceRange(payload, pRanges[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearDepthStencilImage, payload.bytes());
    RVK_TRACE("client vkCmdClearDepthStencilImage -> (async, no reply expected)");
}

void rvk_vkCmdClearAttachments(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    RVK_TRACE("client vkCmdClearAttachments(commandBuffer={:#x}, attachmentCount={}, pAttachments=ptr, rectCount={}, pRects=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(attachmentCount), static_cast<long long>(rectCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        serialize_VkClearAttachment(payload, pAttachments[__i]);
    }
    payload.write_pod(rectCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rectCount); ++__i) {
        serialize_VkClearRect(payload, pRects[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdClearAttachments, payload.bytes());
    RVK_TRACE("client vkCmdClearAttachments -> (async, no reply expected)");
}

void rvk_vkCmdFillBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    RVK_TRACE("client vkCmdFillBuffer(commandBuffer={:#x}, dstBuffer={:#x}, dstOffset={}, size={}, data={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(size), static_cast<long long>(data));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(size);
    payload.write_pod(data);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdFillBuffer, payload.bytes());
    RVK_TRACE("client vkCmdFillBuffer -> (async, no reply expected)");
}

void rvk_vkCmdUpdateBuffer(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    RVK_TRACE("client vkCmdUpdateBuffer(commandBuffer={:#x}, dstBuffer={:#x}, dstOffset={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(dataSize));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(dataSize);
    if (pData != nullptr && dataSize > 0) {
        payload.write(pData, static_cast<std::size_t>(dataSize));
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdUpdateBuffer, payload.bytes());
    RVK_TRACE("client vkCmdUpdateBuffer -> (async, no reply expected)");
}

void rvk_vkCmdSetLineWidth(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, float lineWidth) {
    RVK_TRACE("client vkCmdSetLineWidth(commandBuffer={:#x}, lineWidth={})",::rvk::instr::trace_handle(commandBuffer), lineWidth);
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineWidth);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineWidth, payload.bytes());
    RVK_TRACE("client vkCmdSetLineWidth -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBias(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    RVK_TRACE("client vkCmdSetDepthBias(commandBuffer={:#x}, depthBiasConstantFactor={}, depthBiasClamp={}, depthBiasSlopeFactor={})",::rvk::instr::trace_handle(commandBuffer), depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasConstantFactor);
    payload.write_pod(depthBiasClamp);
    payload.write_pod(depthBiasSlopeFactor);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBias, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBias -> (async, no reply expected)");
}

void rvk_vkCmdSetBlendConstants(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const float* blendConstants) {
    RVK_TRACE("client vkCmdSetBlendConstants(commandBuffer={:#x}, blendConstants=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    for (std::uint32_t __i = 0; __i < 4u; ++__i) {
        payload.write_pod(blendConstants[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetBlendConstants, payload.bytes());
    RVK_TRACE("client vkCmdSetBlendConstants -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBounds(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    RVK_TRACE("client vkCmdSetDepthBounds(commandBuffer={:#x}, minDepthBounds={}, maxDepthBounds={})",::rvk::instr::trace_handle(commandBuffer), minDepthBounds, maxDepthBounds);
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(minDepthBounds);
    payload.write_pod(maxDepthBounds);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBounds, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBounds -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilCompareMask(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    RVK_TRACE("client vkCmdSetStencilCompareMask(commandBuffer={:#x}, faceMask={}, compareMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(compareMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(compareMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilCompareMask, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilCompareMask -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilWriteMask(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    RVK_TRACE("client vkCmdSetStencilWriteMask(commandBuffer={:#x}, faceMask={}, writeMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(writeMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(writeMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilWriteMask, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilWriteMask -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilReference(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    RVK_TRACE("client vkCmdSetStencilReference(commandBuffer={:#x}, faceMask={}, reference={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(reference));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(reference);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilReference, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilReference -> (async, no reply expected)");
}

void rvk_vkCmdDispatch(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    RVK_TRACE("client vkCmdDispatch(commandBuffer={:#x}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatch, payload.bytes());
    RVK_TRACE("client vkCmdDispatch -> (async, no reply expected)");
}

void rvk_vkCmdDispatchIndirect(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    RVK_TRACE("client vkCmdDispatchIndirect(commandBuffer={:#x}, buffer={:#x}, offset={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchIndirect, payload.bytes());
    RVK_TRACE("client vkCmdDispatchIndirect -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndirect(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndirect(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirect, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndirect -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndexedIndirect(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndexedIndirect(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirect, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndexedIndirect -> (async, no reply expected)");
}

void rvk_vkCmdExecuteCommands(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    RVK_TRACE("client vkCmdExecuteCommands(commandBuffer={:#x}, commandBufferCount={}, pCommandBuffers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(commandBufferCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(commandBufferCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(commandBufferCount); ++__i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdExecuteCommands, payload.bytes());
    RVK_TRACE("client vkCmdExecuteCommands -> (async, no reply expected)");
}

VkResult rvk_vkAcquireNextImage2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    RVK_TRACE("client vkAcquireNextImage2KHR(device={:#x}, pAcquireInfo=ptr, pImageIndex=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAcquireNextImageInfoKHR(payload, *pAcquireInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkAcquireNextImage2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkAcquireNextImage2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindBufferMemory2(::rvk::protocol::Transport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    RVK_TRACE("client vkBindBufferMemory2(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindBufferMemory2 -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2(::rvk::protocol::Transport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    RVK_TRACE("client vkBindImageMemory2(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindImageMemory2 -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQuery(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    RVK_TRACE("client vkCmdBeginQuery(commandBuffer={:#x}, queryPool={:#x}, query={}, flags={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginQuery, payload.bytes());
    RVK_TRACE("client vkCmdBeginQuery -> (async, no reply expected)");
}

void rvk_vkCmdBeginRenderPass2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    RVK_TRACE("client vkCmdBeginRenderPass2(commandBuffer={:#x}, pRenderPassBegin=ptr, pSubpassBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2, payload.bytes());
    RVK_TRACE("client vkCmdBeginRenderPass2 -> (async, no reply expected)");
}

void rvk_vkCmdBeginRendering(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    RVK_TRACE("client vkCmdBeginRendering(commandBuffer={:#x}, pRenderingInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRendering, payload.bytes());
    RVK_TRACE("client vkCmdBeginRendering -> (async, no reply expected)");
}

void rvk_vkCmdBindVertexBuffers2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    RVK_TRACE("client vkCmdBindVertexBuffers2(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr, pStrides=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        serialize_VkBuffer(payload, pBuffers[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        payload.write_pod(pOffsets[__i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            payload.write_pod(pSizes[__i]);
        }
    }
    const std::uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            payload.write_pod(pStrides[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2, payload.bytes());
    RVK_TRACE("client vkCmdBindVertexBuffers2 -> (async, no reply expected)");
}

void rvk_vkCmdBlitImage2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    RVK_TRACE("client vkCmdBlitImage2(commandBuffer={:#x}, pBlitImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2, payload.bytes());
    RVK_TRACE("client vkCmdBlitImage2 -> (async, no reply expected)");
}

void rvk_vkCmdCopyBuffer2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    RVK_TRACE("client vkCmdCopyBuffer2(commandBuffer={:#x}, pCopyBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2, payload.bytes());
    RVK_TRACE("client vkCmdCopyBuffer2 -> (async, no reply expected)");
}

void rvk_vkCmdCopyBufferToImage2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    RVK_TRACE("client vkCmdCopyBufferToImage2(commandBuffer={:#x}, pCopyBufferToImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2, payload.bytes());
    RVK_TRACE("client vkCmdCopyBufferToImage2 -> (async, no reply expected)");
}

void rvk_vkCmdCopyImage2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    RVK_TRACE("client vkCmdCopyImage2(commandBuffer={:#x}, pCopyImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2, payload.bytes());
    RVK_TRACE("client vkCmdCopyImage2 -> (async, no reply expected)");
}

void rvk_vkCmdCopyImageToBuffer2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    RVK_TRACE("client vkCmdCopyImageToBuffer2(commandBuffer={:#x}, pCopyImageToBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2, payload.bytes());
    RVK_TRACE("client vkCmdCopyImageToBuffer2 -> (async, no reply expected)");
}

void rvk_vkCmdCopyQueryPoolResults(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    RVK_TRACE("client vkCmdCopyQueryPoolResults(commandBuffer={:#x}, queryPool={:#x}, firstQuery={}, queryCount={}, dstBuffer={:#x}, dstOffset={}, stride={}, flags={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(stride), static_cast<long long>(flags));
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
    RVK_TRACE("client vkCmdCopyQueryPoolResults -> (async, no reply expected)");
}

void rvk_vkCmdDispatchBase(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    RVK_TRACE("client vkCmdDispatchBase(commandBuffer={:#x}, baseGroupX={}, baseGroupY={}, baseGroupZ={}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(baseGroupX), static_cast<long long>(baseGroupY), static_cast<long long>(baseGroupZ), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchBase, payload.bytes());
    RVK_TRACE("client vkCmdDispatchBase -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndexedIndirectCount(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndexedIndirectCount(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndexedIndirectCount -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndirectCount(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndirectCount(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCount, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndirectCount -> (async, no reply expected)");
}

void rvk_vkCmdEndQuery(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    RVK_TRACE("client vkCmdEndQuery(commandBuffer={:#x}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndQuery, payload.bytes());
    RVK_TRACE("client vkCmdEndQuery -> (async, no reply expected)");
}

void rvk_vkCmdEndRenderPass2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    RVK_TRACE("client vkCmdEndRenderPass2(commandBuffer={:#x}, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2, payload.bytes());
    RVK_TRACE("client vkCmdEndRenderPass2 -> (async, no reply expected)");
}

void rvk_vkCmdEndRendering(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer) {
    RVK_TRACE("client vkCmdEndRendering(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRendering, payload.bytes());
    RVK_TRACE("client vkCmdEndRendering -> (async, no reply expected)");
}

void rvk_vkCmdNextSubpass2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    RVK_TRACE("client vkCmdNextSubpass2(commandBuffer={:#x}, pSubpassBeginInfo=ptr, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2, payload.bytes());
    RVK_TRACE("client vkCmdNextSubpass2 -> (async, no reply expected)");
}

void rvk_vkCmdPipelineBarrier2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    RVK_TRACE("client vkCmdPipelineBarrier2(commandBuffer={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2, payload.bytes());
    RVK_TRACE("client vkCmdPipelineBarrier2 -> (async, no reply expected)");
}

void rvk_vkCmdResetEvent2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    RVK_TRACE("client vkCmdResetEvent2(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2, payload.bytes());
    RVK_TRACE("client vkCmdResetEvent2 -> (async, no reply expected)");
}

void rvk_vkCmdResetQueryPool(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    RVK_TRACE("client vkCmdResetQueryPool(commandBuffer={:#x}, queryPool={:#x}, firstQuery={}, queryCount={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetQueryPool, payload.bytes());
    RVK_TRACE("client vkCmdResetQueryPool -> (async, no reply expected)");
}

void rvk_vkCmdResolveImage(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    RVK_TRACE("client vkCmdResolveImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
        serialize_VkImageResolve(payload, pRegions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage, payload.bytes());
    RVK_TRACE("client vkCmdResolveImage -> (async, no reply expected)");
}

void rvk_vkCmdResolveImage2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    RVK_TRACE("client vkCmdResolveImage2(commandBuffer={:#x}, pResolveImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2, payload.bytes());
    RVK_TRACE("client vkCmdResolveImage2 -> (async, no reply expected)");
}

void rvk_vkCmdSetCullMode(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    RVK_TRACE("client vkCmdSetCullMode(commandBuffer={:#x}, cullMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(cullMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCullMode, payload.bytes());
    RVK_TRACE("client vkCmdSetCullMode -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBiasEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    RVK_TRACE("client vkCmdSetDepthBiasEnable(commandBuffer={:#x}, depthBiasEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBiasEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBiasEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBoundsTestEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    RVK_TRACE("client vkCmdSetDepthBoundsTestEnable(commandBuffer={:#x}, depthBoundsTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBoundsTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBoundsTestEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthCompareOp(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    RVK_TRACE("client vkCmdSetDepthCompareOp(commandBuffer={:#x}, depthCompareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthCompareOp));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOp, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthCompareOp -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthTestEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    RVK_TRACE("client vkCmdSetDepthTestEnable(commandBuffer={:#x}, depthTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthTestEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthWriteEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    RVK_TRACE("client vkCmdSetDepthWriteEnable(commandBuffer={:#x}, depthWriteEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthWriteEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthWriteEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetDeviceMask(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    RVK_TRACE("client vkCmdSetDeviceMask(commandBuffer={:#x}, deviceMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(deviceMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMask, payload.bytes());
    RVK_TRACE("client vkCmdSetDeviceMask -> (async, no reply expected)");
}

void rvk_vkCmdSetEvent2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    RVK_TRACE("client vkCmdSetEvent2(commandBuffer={:#x}, event={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2, payload.bytes());
    RVK_TRACE("client vkCmdSetEvent2 -> (async, no reply expected)");
}

void rvk_vkCmdSetFrontFace(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    RVK_TRACE("client vkCmdSetFrontFace(commandBuffer={:#x}, frontFace={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(frontFace));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFace, payload.bytes());
    RVK_TRACE("client vkCmdSetFrontFace -> (async, no reply expected)");
}

void rvk_vkCmdSetPrimitiveRestartEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    RVK_TRACE("client vkCmdSetPrimitiveRestartEnable(commandBuffer={:#x}, primitiveRestartEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveRestartEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetPrimitiveRestartEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetPrimitiveTopology(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    RVK_TRACE("client vkCmdSetPrimitiveTopology(commandBuffer={:#x}, primitiveTopology={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveTopology));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopology, payload.bytes());
    RVK_TRACE("client vkCmdSetPrimitiveTopology -> (async, no reply expected)");
}

void rvk_vkCmdSetRasterizerDiscardEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    RVK_TRACE("client vkCmdSetRasterizerDiscardEnable(commandBuffer={:#x}, rasterizerDiscardEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizerDiscardEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetRasterizerDiscardEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetScissorWithCount(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    RVK_TRACE("client vkCmdSetScissorWithCount(commandBuffer={:#x}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(scissorCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
        serialize_VkRect2D(payload, pScissors[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCount, payload.bytes());
    RVK_TRACE("client vkCmdSetScissorWithCount -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilOp(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    RVK_TRACE("client vkCmdSetStencilOp(commandBuffer={:#x}, faceMask={}, failOp={}, passOp={}, depthFailOp={}, compareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(failOp), static_cast<long long>(passOp), static_cast<long long>(depthFailOp), static_cast<long long>(compareOp));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOp, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilOp -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilTestEnable(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    RVK_TRACE("client vkCmdSetStencilTestEnable(commandBuffer={:#x}, stencilTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stencilTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnable, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilTestEnable -> (async, no reply expected)");
}

void rvk_vkCmdSetViewportWithCount(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    RVK_TRACE("client vkCmdSetViewportWithCount(commandBuffer={:#x}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
        serialize_VkViewport(payload, pViewports[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCount, payload.bytes());
    RVK_TRACE("client vkCmdSetViewportWithCount -> (async, no reply expected)");
}

void rvk_vkCmdWaitEvents2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    RVK_TRACE("client vkCmdWaitEvents2(commandBuffer={:#x}, eventCount={}, pEvents=ptr, pDependencyInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
        serialize_VkEvent(payload, pEvents[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2, payload.bytes());
    RVK_TRACE("client vkCmdWaitEvents2 -> (async, no reply expected)");
}

void rvk_vkCmdWriteTimestamp(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    RVK_TRACE("client vkCmdWriteTimestamp(commandBuffer={:#x}, pipelineStage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineStage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp, payload.bytes());
    RVK_TRACE("client vkCmdWriteTimestamp -> (async, no reply expected)");
}

void rvk_vkCmdWriteTimestamp2(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    RVK_TRACE("client vkCmdWriteTimestamp2(commandBuffer={:#x}, stage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2, payload.bytes());
    RVK_TRACE("client vkCmdWriteTimestamp2 -> (async, no reply expected)");
}

VkResult rvk_vkCreateBufferView(::rvk::protocol::Transport& t, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    RVK_TRACE("client vkCreateBufferView(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pView=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBufferView, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateBufferView -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDescriptorUpdateTemplate(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    RVK_TRACE("client vkCreateDescriptorUpdateTemplate(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorUpdateTemplate=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplate, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDescriptorUpdateTemplate -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePrivateDataSlot(::rvk::protocol::Transport& t, VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    RVK_TRACE("client vkCreatePrivateDataSlot(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPrivateDataSlot=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlotCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePrivateDataSlot, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreatePrivateDataSlot -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateQueryPool(::rvk::protocol::Transport& t, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    RVK_TRACE("client vkCreateQueryPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pQueryPool=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateQueryPool, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateQueryPool -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2(::rvk::protocol::Transport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    RVK_TRACE("client vkCreateRenderPass2(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateRenderPass2 -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversion(::rvk::protocol::Transport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    RVK_TRACE("client vkCreateSamplerYcbcrConversion(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pYcbcrConversion=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversion, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateSamplerYcbcrConversion -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBufferView(::rvk::protocol::Transport& t, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyBufferView(device={:#x}, bufferView={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(bufferView));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferView(payload, bufferView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyBufferView, payload.bytes());
    RVK_TRACE("client vkDestroyBufferView -> (async, no reply expected)");
}

void rvk_vkDestroyDescriptorUpdateTemplate(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDescriptorUpdateTemplate(device={:#x}, descriptorUpdateTemplate={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorUpdateTemplate));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate, payload.bytes());
    RVK_TRACE("client vkDestroyDescriptorUpdateTemplate -> (async, no reply expected)");
}

void rvk_vkDestroyPrivateDataSlot(::rvk::protocol::Transport& t, VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyPrivateDataSlot(device={:#x}, privateDataSlot={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(privateDataSlot));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPrivateDataSlot, payload.bytes());
    RVK_TRACE("client vkDestroyPrivateDataSlot -> (async, no reply expected)");
}

void rvk_vkDestroyQueryPool(::rvk::protocol::Transport& t, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyQueryPool(device={:#x}, queryPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyQueryPool, payload.bytes());
    RVK_TRACE("client vkDestroyQueryPool -> (async, no reply expected)");
}

void rvk_vkDestroySamplerYcbcrConversion(::rvk::protocol::Transport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroySamplerYcbcrConversion(device={:#x}, ycbcrConversion={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(ycbcrConversion));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion, payload.bytes());
    RVK_TRACE("client vkDestroySamplerYcbcrConversion -> (async, no reply expected)");
}

VkResult rvk_vkEnumeratePhysicalDeviceGroups(::rvk::protocol::Transport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    RVK_TRACE("client vkEnumeratePhysicalDeviceGroups(instance={:#x}, pPhysicalDeviceGroupCount=ptr, pPhysicalDeviceGroupProperties=ptr)",::rvk::instr::trace_handle(instance));
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceGroupCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroups, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkEnumeratePhysicalDeviceGroups -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFlushMappedMemoryRanges(::rvk::protocol::Transport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    RVK_TRACE("client vkFlushMappedMemoryRanges(device={:#x}, memoryRangeCount={}, pMemoryRanges=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(memoryRangeCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryRangeCount); ++__i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkFlushMappedMemoryRanges, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkFlushMappedMemoryRanges -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddress(::rvk::protocol::Transport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    RVK_TRACE("client vkGetBufferDeviceAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddress, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetBufferDeviceAddress -> result_code={}", result_code);
    return static_cast<VkDeviceAddress>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2(::rvk::protocol::Transport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetBufferMemoryRequirements2(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetBufferMemoryRequirements2 -> result_code={}", result_code);
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddress(::rvk::protocol::Transport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    RVK_TRACE("client vkGetBufferOpaqueCaptureAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddress, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetBufferOpaqueCaptureAddress -> result_code={}", result_code);
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupport(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    RVK_TRACE("client vkGetDescriptorSetLayoutSupport(device={:#x}, pCreateInfo=ptr, pSupport=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDescriptorSetLayoutSupport -> result_code={}", result_code);
}

void rvk_vkGetDeviceBufferMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceBufferMemoryRequirements(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceBufferMemoryRequirements -> result_code={}", result_code);
}

void rvk_vkGetDeviceGroupPeerMemoryFeatures(::rvk::protocol::Transport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    RVK_TRACE("client vkGetDeviceGroupPeerMemoryFeatures(device={:#x}, heapIndex={}, localDeviceIndex={}, remoteDeviceIndex={}, pPeerMemoryFeatures=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(heapIndex), static_cast<long long>(localDeviceIndex), static_cast<long long>(remoteDeviceIndex));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeatures, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceGroupPeerMemoryFeatures -> result_code={}", result_code);
}

VkResult rvk_vkGetDeviceGroupPresentCapabilitiesKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    RVK_TRACE("client vkGetDeviceGroupPresentCapabilitiesKHR(device={:#x}, pDeviceGroupPresentCapabilities=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(pDeviceGroupPresentCapabilities->sType);
    serialize_pnext_chain(payload, pDeviceGroupPresentCapabilities->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceGroupPresentCapabilitiesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetDeviceGroupSurfacePresentModesKHR(::rvk::protocol::Transport& t, VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    RVK_TRACE("client vkGetDeviceGroupSurfacePresentModesKHR(device={:#x}, surface={:#x}, pModes=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(surface));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSurfaceKHR(payload, surface);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupSurfacePresentModesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceGroupSurfacePresentModesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceImageMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceImageMemoryRequirements(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceImageMemoryRequirements -> result_code={}", result_code);
}

void rvk_vkGetDeviceImageSparseMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceImageSparseMemoryRequirements(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceImageSparseMemoryRequirements -> result_code={}", result_code);
}

void rvk_vkGetDeviceMemoryCommitment(::rvk::protocol::Transport& t, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    RVK_TRACE("client vkGetDeviceMemoryCommitment(device={:#x}, memory={:#x}, pCommittedMemoryInBytes=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryCommitment, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceMemoryCommitment -> result_code={}", result_code);
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddress(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    RVK_TRACE("client vkGetDeviceMemoryOpaqueCaptureAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetDeviceMemoryOpaqueCaptureAddress -> result_code={}", result_code);
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDeviceQueue2(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, std::uint64_t* pQueue) {
    RVK_TRACE("client vkGetDeviceQueue2(device={:#x}, pQueueInfo=ptr, pQueue=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceQueueInfo2(payload, *pQueueInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceQueue2 -> result_code={}", result_code);
}

void rvk_vkGetImageMemoryRequirements2(::rvk::protocol::Transport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetImageMemoryRequirements2(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageMemoryRequirements2 -> result_code={}", result_code);
}

void rvk_vkGetImageSparseMemoryRequirements(::rvk::protocol::Transport& t, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    RVK_TRACE("client vkGetImageSparseMemoryRequirements(device={:#x}, image={:#x}, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSparseMemoryRequirements -> result_code={}", result_code);
}

void rvk_vkGetImageSparseMemoryRequirements2(::rvk::protocol::Transport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    RVK_TRACE("client vkGetImageSparseMemoryRequirements2(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSparseMemoryRequirements2 -> result_code={}", result_code);
}

void rvk_vkGetImageSubresourceLayout(::rvk::protocol::Transport& t, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    RVK_TRACE("client vkGetImageSubresourceLayout(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource(payload, *pSubresource);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSubresourceLayout -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceExternalBufferProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalBufferProperties(physicalDevice={:#x}, pExternalBufferInfo=ptr, pExternalBufferProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalBufferProperties -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceExternalFenceProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalFenceProperties(physicalDevice={:#x}, pExternalFenceInfo=ptr, pExternalFenceProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalFenceProperties -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceExternalSemaphoreProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice={:#x}, pExternalSemaphoreInfo=ptr, pExternalSemaphoreProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalSemaphoreProperties -> result_code={}", result_code);
}

VkResult rvk_vkGetPhysicalDevicePresentRectanglesKHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    RVK_TRACE("client vkGetPhysicalDevicePresentRectanglesKHR(physicalDevice={:#x}, surface={:#x}, pRectCount=ptr, pRects=ptr)",::rvk::instr::trace_handle(physicalDevice), ::rvk::instr::trace_handle(surface));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkSurfaceKHR(payload, surface);
    const std::uint8_t array_present = pRects != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pRectCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDevicePresentRectanglesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDevicePresentRectanglesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetPhysicalDeviceToolProperties(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceToolProperties(physicalDevice={:#x}, pToolCount=ptr, pToolProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pToolProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pToolCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceToolProperties, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceToolProperties -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPrivateData(::rvk::protocol::Transport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    RVK_TRACE("client vkGetPrivateData(device={:#x}, objectType={}, objectHandle={}, privateDataSlot={:#x}, pData=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(objectType), static_cast<long long>(objectHandle), ::rvk::instr::trace_handle(privateDataSlot));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPrivateData, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPrivateData -> result_code={}", result_code);
}

VkResult rvk_vkGetQueryPoolResults(::rvk::protocol::Transport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    RVK_TRACE("client vkGetQueryPoolResults(device={:#x}, queryPool={:#x}, firstQuery={}, queryCount={}, dataSize={}, pData=ptr, stride={}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount), static_cast<long long>(dataSize), static_cast<long long>(stride), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    payload.write_pod<std::uint64_t>(dataSize);
    payload.write_pod(stride);
    payload.write_pod(flags);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetQueryPoolResults, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetQueryPoolResults -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderAreaGranularity(::rvk::protocol::Transport& t, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    RVK_TRACE("client vkGetRenderAreaGranularity(device={:#x}, renderPass={:#x}, pGranularity=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(renderPass));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderAreaGranularity, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetRenderAreaGranularity -> result_code={}", result_code);
}

VkResult rvk_vkGetSemaphoreCounterValue(::rvk::protocol::Transport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    RVK_TRACE("client vkGetSemaphoreCounterValue(device={:#x}, semaphore={:#x}, pValue=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValue, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetSemaphoreCounterValue -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkInvalidateMappedMemoryRanges(::rvk::protocol::Transport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    RVK_TRACE("client vkInvalidateMappedMemoryRanges(device={:#x}, memoryRangeCount={}, pMemoryRanges=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(memoryRangeCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryRangeCount); ++__i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkInvalidateMappedMemoryRanges, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkInvalidateMappedMemoryRanges -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkMergePipelineCaches(::rvk::protocol::Transport& t, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    RVK_TRACE("client vkMergePipelineCaches(device={:#x}, dstCache={:#x}, srcCacheCount={}, pSrcCaches=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(dstCache), static_cast<long long>(srcCacheCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, dstCache);
    payload.write_pod(srcCacheCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(srcCacheCount); ++__i) {
        serialize_VkPipelineCache(payload, pSrcCaches[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkMergePipelineCaches, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkMergePipelineCaches -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueBindSparse(::rvk::protocol::Transport& t, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    RVK_TRACE("client vkQueueBindSparse(queue={:#x}, bindInfoCount={}, pBindInfo=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(bindInfoCount), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
        serialize_VkBindSparseInfo(payload, pBindInfo[__i]);
    }
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueBindSparse, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkQueueBindSparse -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2(::rvk::protocol::Transport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    RVK_TRACE("client vkQueueSubmit2(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
        serialize_VkSubmitInfo2(payload, pSubmits[__i]);
    }
    serialize_VkFence(payload, fence);
    (void)t.send_async(::av::vkproto::kCmd_vkQueueSubmit2, payload.bytes());
    RVK_TRACE("client vkQueueSubmit2 -> (async, no reply expected)");
    return VK_SUCCESS;
}

void rvk_vkResetQueryPool(::rvk::protocol::Transport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    RVK_TRACE("client vkResetQueryPool(device={:#x}, queryPool={:#x}, firstQuery={}, queryCount={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    (void)t.send_async(::av::vkproto::kCmd_vkResetQueryPool, payload.bytes());
    RVK_TRACE("client vkResetQueryPool -> (async, no reply expected)");
}

VkResult rvk_vkSetPrivateData(::rvk::protocol::Transport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    RVK_TRACE("client vkSetPrivateData(device={:#x}, objectType={}, objectHandle={}, privateDataSlot={:#x}, data={})",::rvk::instr::trace_handle(device), static_cast<long long>(objectType), static_cast<long long>(objectHandle), ::rvk::instr::trace_handle(privateDataSlot), static_cast<long long>(data));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    payload.write_pod(data);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSetPrivateData, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkSetPrivateData -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphore(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    RVK_TRACE("client vkSignalSemaphore(device={:#x}, pSignalInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphore, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkSignalSemaphore -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPool(::rvk::protocol::Transport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    RVK_TRACE("client vkTrimCommandPool(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkTrimCommandPool, payload.bytes());
    RVK_TRACE("client vkTrimCommandPool -> (async, no reply expected)");
}

VkResult rvk_vkWaitSemaphores(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    RVK_TRACE("client vkWaitSemaphores(device={:#x}, pWaitInfo=ptr, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(timeout));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphores, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkWaitSemaphores -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    RVK_TRACE("client vkGetPhysicalDeviceFeatures2KHR(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFeatures2KHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceFormatProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties2KHR(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFormatProperties2KHR -> result_code={}", result_code);
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties2KHR(physicalDevice={:#x}, pImageFormatInfo=ptr, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceImageFormatProperties2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties2KHR(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceMemoryProperties2KHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceProperties2KHR(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceProperties2KHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pQueueFamilyPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceQueueFamilyProperties2KHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice={:#x}, pFormatInfo=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const std::uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPropertyCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceSparseImageFormatProperties2KHR -> result_code={}", result_code);
}

VkResult rvk_vkBindBufferMemory2KHR(::rvk::protocol::Transport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    RVK_TRACE("client vkBindBufferMemory2KHR(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindBufferMemory2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2KHR(::rvk::protocol::Transport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    RVK_TRACE("client vkBindImageMemory2KHR(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBindImageMemory2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetBufferMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetBufferMemoryRequirements2KHR -> result_code={}", result_code);
}

void rvk_vkGetImageMemoryRequirements2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetImageMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageMemoryRequirements2KHR -> result_code={}", result_code);
}

void rvk_vkGetImageSparseMemoryRequirements2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    RVK_TRACE("client vkGetImageSparseMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSparseMemoryRequirements2KHR -> result_code={}", result_code);
}

void rvk_vkCmdBeginRenderPass2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    RVK_TRACE("client vkCmdBeginRenderPass2KHR(commandBuffer={:#x}, pRenderPassBegin=ptr, pSubpassBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR, payload.bytes());
    RVK_TRACE("client vkCmdBeginRenderPass2KHR -> (async, no reply expected)");
}

void rvk_vkCmdBeginRenderingKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    RVK_TRACE("client vkCmdBeginRenderingKHR(commandBuffer={:#x}, pRenderingInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderingKHR, payload.bytes());
    RVK_TRACE("client vkCmdBeginRenderingKHR -> (async, no reply expected)");
}

void rvk_vkCmdBlitImage2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    RVK_TRACE("client vkCmdBlitImage2KHR(commandBuffer={:#x}, pBlitImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2KHR, payload.bytes());
    RVK_TRACE("client vkCmdBlitImage2KHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyBuffer2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    RVK_TRACE("client vkCmdCopyBuffer2KHR(commandBuffer={:#x}, pCopyBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2KHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyBuffer2KHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyBufferToImage2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    RVK_TRACE("client vkCmdCopyBufferToImage2KHR(commandBuffer={:#x}, pCopyBufferToImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyBufferToImage2KHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyImage2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    RVK_TRACE("client vkCmdCopyImage2KHR(commandBuffer={:#x}, pCopyImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2KHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyImage2KHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyImageToBuffer2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    RVK_TRACE("client vkCmdCopyImageToBuffer2KHR(commandBuffer={:#x}, pCopyImageToBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyImageToBuffer2KHR -> (async, no reply expected)");
}

void rvk_vkCmdDispatchBaseKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    RVK_TRACE("client vkCmdDispatchBaseKHR(commandBuffer={:#x}, baseGroupX={}, baseGroupY={}, baseGroupZ={}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(baseGroupX), static_cast<long long>(baseGroupY), static_cast<long long>(baseGroupZ), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDispatchBaseKHR, payload.bytes());
    RVK_TRACE("client vkCmdDispatchBaseKHR -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndexedIndirectCountKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndexedIndirectCountKHR(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndexedIndirectCountKHR -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndirectCountKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawIndirectCountKHR(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndirectCountKHR -> (async, no reply expected)");
}

void rvk_vkCmdEndRenderPass2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    RVK_TRACE("client vkCmdEndRenderPass2KHR(commandBuffer={:#x}, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2KHR, payload.bytes());
    RVK_TRACE("client vkCmdEndRenderPass2KHR -> (async, no reply expected)");
}

void rvk_vkCmdEndRenderingKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer) {
    RVK_TRACE("client vkCmdEndRenderingKHR(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndRenderingKHR, payload.bytes());
    RVK_TRACE("client vkCmdEndRenderingKHR -> (async, no reply expected)");
}

void rvk_vkCmdNextSubpass2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    RVK_TRACE("client vkCmdNextSubpass2KHR(commandBuffer={:#x}, pSubpassBeginInfo=ptr, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2KHR, payload.bytes());
    RVK_TRACE("client vkCmdNextSubpass2KHR -> (async, no reply expected)");
}

void rvk_vkCmdPipelineBarrier2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    RVK_TRACE("client vkCmdPipelineBarrier2KHR(commandBuffer={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR, payload.bytes());
    RVK_TRACE("client vkCmdPipelineBarrier2KHR -> (async, no reply expected)");
}

void rvk_vkCmdResetEvent2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    RVK_TRACE("client vkCmdResetEvent2KHR(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2KHR, payload.bytes());
    RVK_TRACE("client vkCmdResetEvent2KHR -> (async, no reply expected)");
}

void rvk_vkCmdResolveImage2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    RVK_TRACE("client vkCmdResolveImage2KHR(commandBuffer={:#x}, pResolveImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2KHR, payload.bytes());
    RVK_TRACE("client vkCmdResolveImage2KHR -> (async, no reply expected)");
}

void rvk_vkCmdSetDeviceMaskKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    RVK_TRACE("client vkCmdSetDeviceMaskKHR(commandBuffer={:#x}, deviceMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(deviceMask));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR, payload.bytes());
    RVK_TRACE("client vkCmdSetDeviceMaskKHR -> (async, no reply expected)");
}

void rvk_vkCmdSetEvent2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    RVK_TRACE("client vkCmdSetEvent2KHR(commandBuffer={:#x}, event={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2KHR, payload.bytes());
    RVK_TRACE("client vkCmdSetEvent2KHR -> (async, no reply expected)");
}

void rvk_vkCmdWaitEvents2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    RVK_TRACE("client vkCmdWaitEvents2KHR(commandBuffer={:#x}, eventCount={}, pEvents=ptr, pDependencyInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
        serialize_VkEvent(payload, pEvents[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2KHR, payload.bytes());
    RVK_TRACE("client vkCmdWaitEvents2KHR -> (async, no reply expected)");
}

void rvk_vkCmdWriteTimestamp2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    RVK_TRACE("client vkCmdWriteTimestamp2KHR(commandBuffer={:#x}, stage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR, payload.bytes());
    RVK_TRACE("client vkCmdWriteTimestamp2KHR -> (async, no reply expected)");
}

VkResult rvk_vkCreateDescriptorUpdateTemplateKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    RVK_TRACE("client vkCreateDescriptorUpdateTemplateKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorUpdateTemplate=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplateKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDescriptorUpdateTemplateKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    RVK_TRACE("client vkCreateRenderPass2KHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateRenderPass2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversionKHR(::rvk::protocol::Transport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    RVK_TRACE("client vkCreateSamplerYcbcrConversionKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pYcbcrConversion=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversionKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateSamplerYcbcrConversionKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorUpdateTemplateKHR(::rvk::protocol::Transport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDescriptorUpdateTemplateKHR(device={:#x}, descriptorUpdateTemplate={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorUpdateTemplate));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR, payload.bytes());
    RVK_TRACE("client vkDestroyDescriptorUpdateTemplateKHR -> (async, no reply expected)");
}

void rvk_vkDestroySamplerYcbcrConversionKHR(::rvk::protocol::Transport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroySamplerYcbcrConversionKHR(device={:#x}, ycbcrConversion={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(ycbcrConversion));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR, payload.bytes());
    RVK_TRACE("client vkDestroySamplerYcbcrConversionKHR -> (async, no reply expected)");
}

VkResult rvk_vkEnumeratePhysicalDeviceGroupsKHR(::rvk::protocol::Transport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    RVK_TRACE("client vkEnumeratePhysicalDeviceGroupsKHR(instance={:#x}, pPhysicalDeviceGroupCount=ptr, pPhysicalDeviceGroupProperties=ptr)",::rvk::instr::trace_handle(instance));
    ::av::WireWriter payload;
    serialize_VkInstance(payload, instance);
    const std::uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pPhysicalDeviceGroupCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroupsKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkEnumeratePhysicalDeviceGroupsKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddressKHR(::rvk::protocol::Transport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    RVK_TRACE("client vkGetBufferDeviceAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddressKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetBufferDeviceAddressKHR -> result_code={}", result_code);
    return static_cast<VkDeviceAddress>(result_code);
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddressKHR(::rvk::protocol::Transport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    RVK_TRACE("client vkGetBufferOpaqueCaptureAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetBufferOpaqueCaptureAddressKHR -> result_code={}", result_code);
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupportKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    RVK_TRACE("client vkGetDescriptorSetLayoutSupportKHR(device={:#x}, pCreateInfo=ptr, pSupport=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDescriptorSetLayoutSupportKHR -> result_code={}", result_code);
}

void rvk_vkGetDeviceBufferMemoryRequirementsKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceBufferMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceBufferMemoryRequirementsKHR -> result_code={}", result_code);
}

void rvk_vkGetDeviceGroupPeerMemoryFeaturesKHR(::rvk::protocol::Transport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    RVK_TRACE("client vkGetDeviceGroupPeerMemoryFeaturesKHR(device={:#x}, heapIndex={}, localDeviceIndex={}, remoteDeviceIndex={}, pPeerMemoryFeatures=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(heapIndex), static_cast<long long>(localDeviceIndex), static_cast<long long>(remoteDeviceIndex));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeaturesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceGroupPeerMemoryFeaturesKHR -> result_code={}", result_code);
}

void rvk_vkGetDeviceImageMemoryRequirementsKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceImageMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceImageMemoryRequirementsKHR -> result_code={}", result_code);
}

void rvk_vkGetDeviceImageSparseMemoryRequirementsKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    RVK_TRACE("client vkGetDeviceImageSparseMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const std::uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pSparseMemoryRequirementCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirementsKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceImageSparseMemoryRequirementsKHR -> result_code={}", result_code);
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddressKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    RVK_TRACE("client vkGetDeviceMemoryOpaqueCaptureAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetDeviceMemoryOpaqueCaptureAddressKHR -> result_code={}", result_code);
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetPhysicalDeviceExternalBufferPropertiesKHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice={:#x}, pExternalBufferInfo=ptr, pExternalBufferProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalBufferPropertiesKHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceExternalFencePropertiesKHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalFencePropertiesKHR(physicalDevice={:#x}, pExternalFenceInfo=ptr, pExternalFenceProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalFencePropertiesKHR -> result_code={}", result_code);
}

void rvk_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    RVK_TRACE("client vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice={:#x}, pExternalSemaphoreInfo=ptr, pExternalSemaphoreProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceExternalSemaphorePropertiesKHR -> result_code={}", result_code);
}

VkResult rvk_vkGetSemaphoreCounterValueKHR(::rvk::protocol::Transport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    RVK_TRACE("client vkGetSemaphoreCounterValueKHR(device={:#x}, semaphore={:#x}, pValue=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValueKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetSemaphoreCounterValueKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2KHR(::rvk::protocol::Transport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    RVK_TRACE("client vkQueueSubmit2KHR(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
    ::av::WireWriter payload;
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
        serialize_VkSubmitInfo2(payload, pSubmits[__i]);
    }
    serialize_VkFence(payload, fence);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkQueueSubmit2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkQueueSubmit2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphoreKHR(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    RVK_TRACE("client vkSignalSemaphoreKHR(device={:#x}, pSignalInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphoreKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkSignalSemaphoreKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPoolKHR(::rvk::protocol::Transport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    RVK_TRACE("client vkTrimCommandPoolKHR(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    (void)t.send_async(::av::vkproto::kCmd_vkTrimCommandPoolKHR, payload.bytes());
    RVK_TRACE("client vkTrimCommandPoolKHR -> (async, no reply expected)");
}

VkResult rvk_vkWaitSemaphoresKHR(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    RVK_TRACE("client vkWaitSemaphoresKHR(device={:#x}, pWaitInfo=ptr, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(timeout));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphoresKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkWaitSemaphoresKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginConditionalRenderingEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    RVK_TRACE("client vkCmdBeginConditionalRenderingEXT(commandBuffer={:#x}, pConditionalRenderingBegin=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkConditionalRenderingBeginInfoEXT(payload, *pConditionalRenderingBegin);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT, payload.bytes());
    RVK_TRACE("client vkCmdBeginConditionalRenderingEXT -> (async, no reply expected)");
}

void rvk_vkCmdEndConditionalRenderingEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer) {
    RVK_TRACE("client vkCmdEndConditionalRenderingEXT(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT, payload.bytes());
    RVK_TRACE("client vkCmdEndConditionalRenderingEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetColorWriteEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    RVK_TRACE("client vkCmdSetColorWriteEnableEXT(commandBuffer={:#x}, attachmentCount={}, pColorWriteEnables=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(attachmentCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        payload.write_pod(pColorWriteEnables[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetColorWriteEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBiasEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    RVK_TRACE("client vkCmdSetDepthBiasEnableEXT(commandBuffer={:#x}, depthBiasEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBiasEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBiasEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetLogicOpEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    RVK_TRACE("client vkCmdSetLogicOpEXT(commandBuffer={:#x}, logicOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(logicOp));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetLogicOpEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetPatchControlPointsEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    RVK_TRACE("client vkCmdSetPatchControlPointsEXT(commandBuffer={:#x}, patchControlPoints={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(patchControlPoints));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(patchControlPoints);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetPatchControlPointsEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetPrimitiveRestartEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    RVK_TRACE("client vkCmdSetPrimitiveRestartEnableEXT(commandBuffer={:#x}, primitiveRestartEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveRestartEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetPrimitiveRestartEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetRasterizerDiscardEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    RVK_TRACE("client vkCmdSetRasterizerDiscardEnableEXT(commandBuffer={:#x}, rasterizerDiscardEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizerDiscardEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetRasterizerDiscardEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetVertexInputEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    RVK_TRACE("client vkCmdSetVertexInputEXT(commandBuffer={:#x}, vertexBindingDescriptionCount={}, pVertexBindingDescriptions=ptr, vertexAttributeDescriptionCount={}, pVertexAttributeDescriptions=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(vertexBindingDescriptionCount), static_cast<long long>(vertexAttributeDescriptionCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexBindingDescriptionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(vertexBindingDescriptionCount); ++__i) {
        serialize_VkVertexInputBindingDescription2EXT(payload, pVertexBindingDescriptions[__i]);
    }
    payload.write_pod(vertexAttributeDescriptionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(vertexAttributeDescriptionCount); ++__i) {
        serialize_VkVertexInputAttributeDescription2EXT(payload, pVertexAttributeDescriptions[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetVertexInputEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetVertexInputEXT -> (async, no reply expected)");
}

VkResult rvk_vkBuildAccelerationStructuresKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    RVK_TRACE("client vkBuildAccelerationStructuresKHR(device={:#x}, deferredOperation={:#x}, infoCount={}, pInfos=ptr, ppBuildRangeInfos=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation), static_cast<long long>(infoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    payload.write_pod(infoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        const std::uint32_t __n = static_cast<std::uint32_t>(pInfos[__i].geometryCount);
        for (std::uint32_t __j = 0; __j < __n; ++__j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[__i][__j]);
        }
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkBuildAccelerationStructuresKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkBuildAccelerationStructuresKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBindShadersEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
    RVK_TRACE("client vkCmdBindShadersEXT(commandBuffer={:#x}, stageCount={}, pStages=ptr, pShaders=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stageCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stageCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(stageCount); ++__i) {
        payload.write_pod(pStages[__i]);
    }
    const std::uint8_t pShaders_present = pShaders != nullptr ? 1 : 0;
    payload.write_pod(pShaders_present);
    if (pShaders_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(stageCount); ++__i) {
            serialize_VkShaderEXT(payload, pShaders[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindShadersEXT, payload.bytes());
    RVK_TRACE("client vkCmdBindShadersEXT -> (async, no reply expected)");
}

void rvk_vkCmdBindVertexBuffers2EXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    RVK_TRACE("client vkCmdBindVertexBuffers2EXT(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr, pStrides=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        serialize_VkBuffer(payload, pBuffers[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        payload.write_pod(pOffsets[__i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            payload.write_pod(pSizes[__i]);
        }
    }
    const std::uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            payload.write_pod(pStrides[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT, payload.bytes());
    RVK_TRACE("client vkCmdBindVertexBuffers2EXT -> (async, no reply expected)");
}

void rvk_vkCmdBuildAccelerationStructuresIndirectKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) {
    RVK_TRACE("client vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer={:#x}, infoCount={}, pInfos=ptr, pIndirectDeviceAddresses=ptr, pIndirectStrides=ptr, ppMaxPrimitiveCounts=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(infoCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        payload.write_pod(pIndirectDeviceAddresses[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        payload.write_pod(pIndirectStrides[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        const std::uint32_t __n = static_cast<std::uint32_t>(pInfos[__i].geometryCount);
        for (std::uint32_t __j = 0; __j < __n; ++__j) {
            payload.write_pod(ppMaxPrimitiveCounts[__i][__j]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR, payload.bytes());
    RVK_TRACE("client vkCmdBuildAccelerationStructuresIndirectKHR -> (async, no reply expected)");
}

void rvk_vkCmdBuildAccelerationStructuresKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    RVK_TRACE("client vkCmdBuildAccelerationStructuresKHR(commandBuffer={:#x}, infoCount={}, pInfos=ptr, ppBuildRangeInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(infoCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
        const std::uint32_t __n = static_cast<std::uint32_t>(pInfos[__i].geometryCount);
        for (std::uint32_t __j = 0; __j < __n; ++__j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[__i][__j]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR, payload.bytes());
    RVK_TRACE("client vkCmdBuildAccelerationStructuresKHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyAccelerationStructureKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    RVK_TRACE("client vkCmdCopyAccelerationStructureKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyAccelerationStructureKHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyAccelerationStructureToMemoryKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    RVK_TRACE("client vkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyAccelerationStructureToMemoryKHR -> (async, no reply expected)");
}

void rvk_vkCmdCopyMemoryToAccelerationStructureKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    RVK_TRACE("client vkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR, payload.bytes());
    RVK_TRACE("client vkCmdCopyMemoryToAccelerationStructureKHR -> (async, no reply expected)");
}

void rvk_vkCmdDrawMeshTasksEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    RVK_TRACE("client vkCmdDrawMeshTasksEXT(commandBuffer={:#x}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT, payload.bytes());
    RVK_TRACE("client vkCmdDrawMeshTasksEXT -> (async, no reply expected)");
}

void rvk_vkCmdDrawMeshTasksIndirectCountEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT, payload.bytes());
    RVK_TRACE("client vkCmdDrawMeshTasksIndirectCountEXT -> (async, no reply expected)");
}

void rvk_vkCmdDrawMeshTasksIndirectEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    RVK_TRACE("client vkCmdDrawMeshTasksIndirectEXT(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT, payload.bytes());
    RVK_TRACE("client vkCmdDrawMeshTasksIndirectEXT -> (async, no reply expected)");
}

void rvk_vkCmdPushDescriptorSetKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    RVK_TRACE("client vkCmdPushDescriptorSetKHR(commandBuffer={:#x}, pipelineBindPoint={}, layout={:#x}, set={}, descriptorWriteCount={}, pDescriptorWrites=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(layout), static_cast<long long>(set), static_cast<long long>(descriptorWriteCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(set);
    payload.write_pod(descriptorWriteCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorWriteCount); ++__i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR, payload.bytes());
    RVK_TRACE("client vkCmdPushDescriptorSetKHR -> (async, no reply expected)");
}

void rvk_vkCmdSetAlphaToCoverageEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    RVK_TRACE("client vkCmdSetAlphaToCoverageEnableEXT(commandBuffer={:#x}, alphaToCoverageEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(alphaToCoverageEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToCoverageEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetAlphaToCoverageEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetAlphaToOneEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    RVK_TRACE("client vkCmdSetAlphaToOneEnableEXT(commandBuffer={:#x}, alphaToOneEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(alphaToOneEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToOneEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetAlphaToOneEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetColorBlendAdvancedEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    RVK_TRACE("client vkCmdSetColorBlendAdvancedEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendAdvanced=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        serialize_VkColorBlendAdvancedEXT(payload, pColorBlendAdvanced[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetColorBlendAdvancedEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetColorBlendEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    RVK_TRACE("client vkCmdSetColorBlendEnableEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendEnables=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        payload.write_pod(pColorBlendEnables[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetColorBlendEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetColorBlendEquationEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    RVK_TRACE("client vkCmdSetColorBlendEquationEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendEquations=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        serialize_VkColorBlendEquationEXT(payload, pColorBlendEquations[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetColorBlendEquationEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetColorWriteMaskEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    RVK_TRACE("client vkCmdSetColorWriteMaskEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorWriteMasks=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
        payload.write_pod(pColorWriteMasks[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetColorWriteMaskEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetConservativeRasterizationModeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    RVK_TRACE("client vkCmdSetConservativeRasterizationModeEXT(commandBuffer={:#x}, conservativeRasterizationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(conservativeRasterizationMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(conservativeRasterizationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetConservativeRasterizationModeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageModulationModeNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    RVK_TRACE("client vkCmdSetCoverageModulationModeNV(commandBuffer={:#x}, coverageModulationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageModulationModeNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageModulationTableEnableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    RVK_TRACE("client vkCmdSetCoverageModulationTableEnableNV(commandBuffer={:#x}, coverageModulationTableEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationTableEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageModulationTableEnableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageModulationTableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    RVK_TRACE("client vkCmdSetCoverageModulationTableNV(commandBuffer={:#x}, coverageModulationTableCount={}, pCoverageModulationTable=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationTableCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(coverageModulationTableCount); ++__i) {
        payload.write_pod(pCoverageModulationTable[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageModulationTableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageReductionModeNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    RVK_TRACE("client vkCmdSetCoverageReductionModeNV(commandBuffer={:#x}, coverageReductionMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageReductionMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageReductionMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageReductionModeNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageToColorEnableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    RVK_TRACE("client vkCmdSetCoverageToColorEnableNV(commandBuffer={:#x}, coverageToColorEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageToColorEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageToColorEnableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCoverageToColorLocationNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    RVK_TRACE("client vkCmdSetCoverageToColorLocationNV(commandBuffer={:#x}, coverageToColorLocation={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageToColorLocation));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorLocation);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV, payload.bytes());
    RVK_TRACE("client vkCmdSetCoverageToColorLocationNV -> (async, no reply expected)");
}

void rvk_vkCmdSetCullModeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    RVK_TRACE("client vkCmdSetCullModeEXT(commandBuffer={:#x}, cullMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(cullMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetCullModeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetCullModeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthBoundsTestEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    RVK_TRACE("client vkCmdSetDepthBoundsTestEnableEXT(commandBuffer={:#x}, depthBoundsTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBoundsTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthBoundsTestEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthClampEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    RVK_TRACE("client vkCmdSetDepthClampEnableEXT(commandBuffer={:#x}, depthClampEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClampEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthClampEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthClampRangeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
    RVK_TRACE("client vkCmdSetDepthClampRangeEXT(commandBuffer={:#x}, depthClampMode={}, pDepthClampRange=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClampMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampMode);
    serialize_VkDepthClampRangeEXT(payload, *pDepthClampRange);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthClampRangeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthClipEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    RVK_TRACE("client vkCmdSetDepthClipEnableEXT(commandBuffer={:#x}, depthClipEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClipEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClipEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthClipEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthClipNegativeOneToOneEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    RVK_TRACE("client vkCmdSetDepthClipNegativeOneToOneEXT(commandBuffer={:#x}, negativeOneToOne={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(negativeOneToOne));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(negativeOneToOne);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthClipNegativeOneToOneEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthCompareOpEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    RVK_TRACE("client vkCmdSetDepthCompareOpEXT(commandBuffer={:#x}, depthCompareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthCompareOp));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthCompareOpEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthTestEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    RVK_TRACE("client vkCmdSetDepthTestEnableEXT(commandBuffer={:#x}, depthTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthTestEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetDepthWriteEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    RVK_TRACE("client vkCmdSetDepthWriteEnableEXT(commandBuffer={:#x}, depthWriteEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthWriteEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetDepthWriteEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetExtraPrimitiveOverestimationSizeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    RVK_TRACE("client vkCmdSetExtraPrimitiveOverestimationSizeEXT(commandBuffer={:#x}, extraPrimitiveOverestimationSize={})",::rvk::instr::trace_handle(commandBuffer), extraPrimitiveOverestimationSize);
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(extraPrimitiveOverestimationSize);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetExtraPrimitiveOverestimationSizeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetFragmentShadingRateKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) {
    RVK_TRACE("client vkCmdSetFragmentShadingRateKHR(commandBuffer={:#x}, pFragmentSize=ptr, combinerOps=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkExtent2D(payload, *pFragmentSize);
    for (std::uint32_t __i = 0; __i < 2u; ++__i) {
        payload.write_pod(combinerOps[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR, payload.bytes());
    RVK_TRACE("client vkCmdSetFragmentShadingRateKHR -> (async, no reply expected)");
}

void rvk_vkCmdSetFrontFaceEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    RVK_TRACE("client vkCmdSetFrontFaceEXT(commandBuffer={:#x}, frontFace={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(frontFace));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFaceEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetFrontFaceEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetLineRasterizationModeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    RVK_TRACE("client vkCmdSetLineRasterizationModeEXT(commandBuffer={:#x}, lineRasterizationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(lineRasterizationMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineRasterizationMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetLineRasterizationModeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetLineStippleEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    RVK_TRACE("client vkCmdSetLineStippleEXT(commandBuffer={:#x}, lineStippleFactor={}, lineStipplePattern={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(lineStippleFactor), static_cast<long long>(lineStipplePattern));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineStippleFactor);
    payload.write_pod(lineStipplePattern);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetLineStippleEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetLineStippleEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    RVK_TRACE("client vkCmdSetLineStippleEnableEXT(commandBuffer={:#x}, stippledLineEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stippledLineEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stippledLineEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetLineStippleEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetLogicOpEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    RVK_TRACE("client vkCmdSetLogicOpEnableEXT(commandBuffer={:#x}, logicOpEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(logicOpEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOpEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetLogicOpEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetPolygonModeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    RVK_TRACE("client vkCmdSetPolygonModeEXT(commandBuffer={:#x}, polygonMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(polygonMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(polygonMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPolygonModeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetPolygonModeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetPrimitiveTopologyEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    RVK_TRACE("client vkCmdSetPrimitiveTopologyEXT(commandBuffer={:#x}, primitiveTopology={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveTopology));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetPrimitiveTopologyEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetProvokingVertexModeEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    RVK_TRACE("client vkCmdSetProvokingVertexModeEXT(commandBuffer={:#x}, provokingVertexMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(provokingVertexMode));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(provokingVertexMode);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetProvokingVertexModeEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetRasterizationSamplesEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    RVK_TRACE("client vkCmdSetRasterizationSamplesEXT(commandBuffer={:#x}, rasterizationSamples={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizationSamples));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationSamples);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetRasterizationSamplesEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetRasterizationStreamEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    RVK_TRACE("client vkCmdSetRasterizationStreamEXT(commandBuffer={:#x}, rasterizationStream={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizationStream));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationStream);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetRasterizationStreamEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetRayTracingPipelineStackSizeKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
    RVK_TRACE("client vkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer={:#x}, pipelineStackSize={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineStackSize));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStackSize);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR, payload.bytes());
    RVK_TRACE("client vkCmdSetRayTracingPipelineStackSizeKHR -> (async, no reply expected)");
}

void rvk_vkCmdSetRepresentativeFragmentTestEnableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    RVK_TRACE("client vkCmdSetRepresentativeFragmentTestEnableNV(commandBuffer={:#x}, representativeFragmentTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(representativeFragmentTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(representativeFragmentTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetRepresentativeFragmentTestEnableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetSampleLocationsEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    RVK_TRACE("client vkCmdSetSampleLocationsEnableEXT(commandBuffer={:#x}, sampleLocationsEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(sampleLocationsEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(sampleLocationsEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetSampleLocationsEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetSampleMaskEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    RVK_TRACE("client vkCmdSetSampleMaskEXT(commandBuffer={:#x}, samples={}, pSampleMask=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(samples));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(samples);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>((samples + 31) / 32); ++__i) {
        payload.write_pod(pSampleMask[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetSampleMaskEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetSampleMaskEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetScissorWithCountEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    RVK_TRACE("client vkCmdSetScissorWithCountEXT(commandBuffer={:#x}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(scissorCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
        serialize_VkRect2D(payload, pScissors[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetScissorWithCountEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetShadingRateImageEnableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    RVK_TRACE("client vkCmdSetShadingRateImageEnableNV(commandBuffer={:#x}, shadingRateImageEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(shadingRateImageEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(shadingRateImageEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetShadingRateImageEnableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilOpEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    RVK_TRACE("client vkCmdSetStencilOpEXT(commandBuffer={:#x}, faceMask={}, failOp={}, passOp={}, depthFailOp={}, compareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(failOp), static_cast<long long>(passOp), static_cast<long long>(depthFailOp), static_cast<long long>(compareOp));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOpEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilOpEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetStencilTestEnableEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    RVK_TRACE("client vkCmdSetStencilTestEnableEXT(commandBuffer={:#x}, stencilTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stencilTestEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetStencilTestEnableEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetTessellationDomainOriginEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    RVK_TRACE("client vkCmdSetTessellationDomainOriginEXT(commandBuffer={:#x}, domainOrigin={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(domainOrigin));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(domainOrigin);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetTessellationDomainOriginEXT -> (async, no reply expected)");
}

void rvk_vkCmdSetViewportSwizzleNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    RVK_TRACE("client vkCmdSetViewportSwizzleNV(commandBuffer={:#x}, firstViewport={}, viewportCount={}, pViewportSwizzles=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstViewport), static_cast<long long>(viewportCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
        serialize_VkViewportSwizzleNV(payload, pViewportSwizzles[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV, payload.bytes());
    RVK_TRACE("client vkCmdSetViewportSwizzleNV -> (async, no reply expected)");
}

void rvk_vkCmdSetViewportWScalingEnableNV(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    RVK_TRACE("client vkCmdSetViewportWScalingEnableNV(commandBuffer={:#x}, viewportWScalingEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportWScalingEnable));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportWScalingEnable);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV, payload.bytes());
    RVK_TRACE("client vkCmdSetViewportWScalingEnableNV -> (async, no reply expected)");
}

void rvk_vkCmdSetViewportWithCountEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    RVK_TRACE("client vkCmdSetViewportWithCountEXT(commandBuffer={:#x}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
        serialize_VkViewport(payload, pViewports[__i]);
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT, payload.bytes());
    RVK_TRACE("client vkCmdSetViewportWithCountEXT -> (async, no reply expected)");
}

void rvk_vkCmdTraceRaysIndirectKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
    RVK_TRACE("client vkCmdTraceRaysIndirectKHR(commandBuffer={:#x}, pRaygenShaderBindingTable=ptr, pMissShaderBindingTable=ptr, pHitShaderBindingTable=ptr, pCallableShaderBindingTable=ptr, indirectDeviceAddress={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(indirectDeviceAddress));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(indirectDeviceAddress);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR, payload.bytes());
    RVK_TRACE("client vkCmdTraceRaysIndirectKHR -> (async, no reply expected)");
}

void rvk_vkCmdTraceRaysKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
    RVK_TRACE("client vkCmdTraceRaysKHR(commandBuffer={:#x}, pRaygenShaderBindingTable=ptr, pMissShaderBindingTable=ptr, pHitShaderBindingTable=ptr, pCallableShaderBindingTable=ptr, width={}, height={}, depth={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(width), static_cast<long long>(height), static_cast<long long>(depth));
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
    RVK_TRACE("client vkCmdTraceRaysKHR -> (async, no reply expected)");
}

void rvk_vkCmdWriteAccelerationStructuresPropertiesKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
    RVK_TRACE("client vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer={:#x}, accelerationStructureCount={}, pAccelerationStructures=ptr, queryType={}, queryPool={:#x}, firstQuery={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(accelerationStructureCount), static_cast<long long>(queryType), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(accelerationStructureCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(accelerationStructureCount); ++__i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[__i]);
    }
    payload.write_pod(queryType);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR, payload.bytes());
    RVK_TRACE("client vkCmdWriteAccelerationStructuresPropertiesKHR -> (async, no reply expected)");
}

VkResult rvk_vkCopyAccelerationStructureKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    RVK_TRACE("client vkCopyAccelerationStructureKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyAccelerationStructureKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyAccelerationStructureToMemoryKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    RVK_TRACE("client vkCopyAccelerationStructureToMemoryKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyAccelerationStructureToMemoryKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToImageEXT(::rvk::protocol::Transport& t, VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
    RVK_TRACE("client vkCopyImageToImageEXT(device={:#x}, pCopyImageToImageInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToImageInfo(payload, *pCopyImageToImageInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToImageEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyImageToImageEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToMemoryEXT(::rvk::protocol::Transport& t, VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
    RVK_TRACE("client vkCopyImageToMemoryEXT(device={:#x}, pCopyImageToMemoryInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToMemoryInfo(payload, *pCopyImageToMemoryInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToMemoryEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyImageToMemoryEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToAccelerationStructureKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    RVK_TRACE("client vkCopyMemoryToAccelerationStructureKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyMemoryToAccelerationStructureKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToImageEXT(::rvk::protocol::Transport& t, VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
    RVK_TRACE("client vkCopyMemoryToImageEXT(device={:#x}, pCopyMemoryToImageInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkCopyMemoryToImageInfo(payload, *pCopyMemoryToImageInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToImageEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkCopyMemoryToImageEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateAccelerationStructureKHR(::rvk::protocol::Transport& t, VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) {
    RVK_TRACE("client vkCreateAccelerationStructureKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pAccelerationStructure=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateAccelerationStructureKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateAccelerationStructureKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDeferredOperationKHR(::rvk::protocol::Transport& t, VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) {
    RVK_TRACE("client vkCreateDeferredOperationKHR(device={:#x}, pAllocator=ptr, pDeferredOperation=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDeferredOperationKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreateDeferredOperationKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRayTracingPipelinesKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    RVK_TRACE("client vkCreateRayTracingPipelinesKHR(device={:#x}, deferredOperation={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
        serialize_VkRayTracingPipelineCreateInfoKHR(payload, pCreateInfos[__i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(createInfoCount); ++__k) {
        if (!r.read_pod(pPipelines[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkCreateRayTracingPipelinesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateShadersEXT(::rvk::protocol::Transport& t, VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) {
    RVK_TRACE("client vkCreateShadersEXT(device={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pShaders=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(createInfoCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(createInfoCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
        serialize_VkShaderCreateInfoEXT(payload, pCreateInfos[__i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShadersEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(createInfoCount); ++__k) {
        if (!r.read_pod(pShaders[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    RVK_TRACE("client vkCreateShadersEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkDeferredOperationJoinKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR operation) {
    RVK_TRACE("client vkDeferredOperationJoinKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkDeferredOperationJoinKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkDeferredOperationJoinKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyAccelerationStructureKHR(::rvk::protocol::Transport& t, VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyAccelerationStructureKHR(device={:#x}, accelerationStructure={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(accelerationStructure));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureKHR(payload, accelerationStructure);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR, payload.bytes());
    RVK_TRACE("client vkDestroyAccelerationStructureKHR -> (async, no reply expected)");
}

void rvk_vkDestroyDeferredOperationKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyDeferredOperationKHR(device={:#x}, operation={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyDeferredOperationKHR, payload.bytes());
    RVK_TRACE("client vkDestroyDeferredOperationKHR -> (async, no reply expected)");
}

void rvk_vkDestroyShaderEXT(::rvk::protocol::Transport& t, VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyShaderEXT(device={:#x}, shader={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(shader));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkShaderEXT(payload, shader);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyShaderEXT, payload.bytes());
    RVK_TRACE("client vkDestroyShaderEXT -> (async, no reply expected)");
}

void rvk_vkGetAccelerationStructureBuildSizesKHR(::rvk::protocol::Transport& t, VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
    RVK_TRACE("client vkGetAccelerationStructureBuildSizesKHR(device={:#x}, buildType={}, pBuildInfo=ptr, pMaxPrimitiveCounts=ptr, pSizeInfo=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(buildType));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(buildType);
    serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, *pBuildInfo);
    const std::uint8_t pMaxPrimitiveCounts_present = pMaxPrimitiveCounts != nullptr ? 1 : 0;
    payload.write_pod(pMaxPrimitiveCounts_present);
    if (pMaxPrimitiveCounts_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(pBuildInfo->geometryCount); ++__i) {
            payload.write_pod(pMaxPrimitiveCounts[__i]);
        }
    }
    payload.write_pod(pSizeInfo->sType);
    serialize_pnext_chain(payload, pSizeInfo->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetAccelerationStructureBuildSizesKHR -> result_code={}", result_code);
}

VkDeviceAddress rvk_vkGetAccelerationStructureDeviceAddressKHR(::rvk::protocol::Transport& t, VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
    RVK_TRACE("client vkGetAccelerationStructureDeviceAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureDeviceAddressInfoKHR(payload, *pInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureDeviceAddressKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetAccelerationStructureDeviceAddressKHR -> result_code={}", result_code);
    return static_cast<VkDeviceAddress>(result_code);
}

uint32_t rvk_vkGetDeferredOperationMaxConcurrencyKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR operation) {
    RVK_TRACE("client vkGetDeferredOperationMaxConcurrencyKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationMaxConcurrencyKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetDeferredOperationMaxConcurrencyKHR -> result_code={}", result_code);
    return static_cast<uint32_t>(result_code);
}

VkResult rvk_vkGetDeferredOperationResultKHR(::rvk::protocol::Transport& t, VkDevice device, VkDeferredOperationKHR operation) {
    RVK_TRACE("client vkGetDeferredOperationResultKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationResultKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetDeferredOperationResultKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceAccelerationStructureCompatibilityKHR(::rvk::protocol::Transport& t, VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
    RVK_TRACE("client vkGetDeviceAccelerationStructureCompatibilityKHR(device={:#x}, pVersionInfo=ptr, pCompatibility=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureVersionInfoKHR(payload, *pVersionInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceAccelerationStructureCompatibilityKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceAccelerationStructureCompatibilityKHR -> result_code={}", result_code);
}

void rvk_vkGetImageSubresourceLayout2EXT(::rvk::protocol::Transport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    RVK_TRACE("client vkGetImageSubresourceLayout2EXT(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSubresourceLayout2EXT -> result_code={}", result_code);
}

VkResult rvk_vkGetPhysicalDeviceFragmentShadingRatesKHR(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
    RVK_TRACE("client vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice={:#x}, pFragmentShadingRateCount=ptr, pFragmentShadingRates=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pFragmentShadingRates != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pFragmentShadingRateCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFragmentShadingRatesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceFragmentShadingRatesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(::rvk::protocol::Transport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    RVK_TRACE("client vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device={:#x}, pipeline={:#x}, firstGroup={}, groupCount={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(firstGroup), static_cast<long long>(groupCount), static_cast<long long>(dataSize));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<std::uint64_t>(dataSize);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetRayTracingCaptureReplayShaderGroupHandlesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingShaderGroupHandlesKHR(::rvk::protocol::Transport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    RVK_TRACE("client vkGetRayTracingShaderGroupHandlesKHR(device={:#x}, pipeline={:#x}, firstGroup={}, groupCount={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(firstGroup), static_cast<long long>(groupCount), static_cast<long long>(dataSize));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<std::uint64_t>(dataSize);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupHandlesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetRayTracingShaderGroupHandlesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkDeviceSize rvk_vkGetRayTracingShaderGroupStackSizeKHR(::rvk::protocol::Transport& t, VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
    RVK_TRACE("client vkGetRayTracingShaderGroupStackSizeKHR(device={:#x}, pipeline={:#x}, group={}, groupShader={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(group), static_cast<long long>(groupShader));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(group);
    payload.write_pod(groupShader);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupStackSizeKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetRayTracingShaderGroupStackSizeKHR -> result_code={}", result_code);
    return static_cast<VkDeviceSize>(result_code);
}

VkResult rvk_vkTransitionImageLayoutEXT(::rvk::protocol::Transport& t, VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
    RVK_TRACE("client vkTransitionImageLayoutEXT(device={:#x}, transitionCount={}, pTransitions=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(transitionCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(transitionCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(transitionCount); ++__i) {
        serialize_VkHostImageLayoutTransitionInfo(payload, pTransitions[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkTransitionImageLayoutEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkTransitionImageLayoutEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkWriteAccelerationStructuresPropertiesKHR(::rvk::protocol::Transport& t, VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
    RVK_TRACE("client vkWriteAccelerationStructuresPropertiesKHR(device={:#x}, accelerationStructureCount={}, pAccelerationStructures=ptr, queryType={}, dataSize={}, pData=ptr, stride={})",::rvk::instr::trace_handle(device), static_cast<long long>(accelerationStructureCount), static_cast<long long>(queryType), static_cast<long long>(dataSize), static_cast<long long>(stride));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(accelerationStructureCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(accelerationStructureCount); ++__i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[__i]);
    }
    payload.write_pod(queryType);
    payload.write_pod<std::uint64_t>(dataSize);
    payload.write_pod<std::uint64_t>(stride);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkWriteAccelerationStructuresPropertiesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetCalibratedTimestampsEXT(::rvk::protocol::Transport& t, VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    RVK_TRACE("client vkGetCalibratedTimestampsEXT(device={:#x}, timestampCount={}, pTimestampInfos=ptr, pTimestamps=ptr, pMaxDeviation=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(timestampCount));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(timestampCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(timestampCount); ++__i) {
        serialize_VkCalibratedTimestampInfoKHR(payload, pTimestampInfos[__i]);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>(timestampCount); ++__k) {
        if (!r.read_pod(pTimestamps[__k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    if (!r.read_pod(*pMaxDeviation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkGetCalibratedTimestampsEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(::rvk::protocol::Transport& t, VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    RVK_TRACE("client vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice={:#x}, pTimeDomainCount=ptr, pTimeDomains=ptr)",::rvk::instr::trace_handle(physicalDevice));
    ::av::WireWriter payload;
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const std::uint8_t array_present = pTimeDomains != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<std::uint32_t>(*pTimeDomainCount);
    }
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPhysicalDeviceCalibrateableTimeDomainsEXT -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetRenderingAttachmentLocationsKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
    RVK_TRACE("client vkCmdSetRenderingAttachmentLocationsKHR(commandBuffer={:#x}, pLocationInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingAttachmentLocationInfo(payload, *pLocationInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR, payload.bytes());
    RVK_TRACE("client vkCmdSetRenderingAttachmentLocationsKHR -> (async, no reply expected)");
}

void rvk_vkCmdSetRenderingInputAttachmentIndicesKHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
    RVK_TRACE("client vkCmdSetRenderingInputAttachmentIndicesKHR(commandBuffer={:#x}, pInputAttachmentIndexInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInputAttachmentIndexInfo(payload, *pInputAttachmentIndexInfo);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR, payload.bytes());
    RVK_TRACE("client vkCmdSetRenderingInputAttachmentIndicesKHR -> (async, no reply expected)");
}

void rvk_vkCmdBindIndexBuffer2KHR(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
    RVK_TRACE("client vkCmdBindIndexBuffer2KHR(commandBuffer={:#x}, buffer={:#x}, offset={}, size={}, indexType={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(size), static_cast<long long>(indexType));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(size);
    payload.write_pod(indexType);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR, payload.bytes());
    RVK_TRACE("client vkCmdBindIndexBuffer2KHR -> (async, no reply expected)");
}

VkResult rvk_vkCreatePipelineBinariesKHR(::rvk::protocol::Transport& t, VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
    RVK_TRACE("client vkCreatePipelineBinariesKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pBinaries=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    payload.write_pod(pBinaries->sType);
    serialize_pnext_chain(payload, pBinaries->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineBinariesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkCreatePipelineBinariesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineBinaryKHR(::rvk::protocol::Transport& t, VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkDestroyPipelineBinaryKHR(device={:#x}, pipelineBinary={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineBinary));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryKHR(payload, pipelineBinary);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    (void)t.send_async(::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR, payload.bytes());
    RVK_TRACE("client vkDestroyPipelineBinaryKHR -> (async, no reply expected)");
}

void rvk_vkGetDeviceImageSubresourceLayoutKHR(::rvk::protocol::Transport& t, VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) {
    RVK_TRACE("client vkGetDeviceImageSubresourceLayoutKHR(device={:#x}, pInfo=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageSubresourceInfo(payload, *pInfo);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetDeviceImageSubresourceLayoutKHR -> result_code={}", result_code);
}

void rvk_vkGetImageSubresourceLayout2KHR(::rvk::protocol::Transport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    RVK_TRACE("client vkGetImageSubresourceLayout2KHR(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetImageSubresourceLayout2KHR -> result_code={}", result_code);
}

VkResult rvk_vkGetPipelineKeyKHR(::rvk::protocol::Transport& t, VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
    RVK_TRACE("client vkGetPipelineKeyKHR(device={:#x}, pPipelineCreateInfo=ptr, pPipelineKey=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCreateInfoKHR(payload, *pPipelineCreateInfo);
    payload.write_pod(pPipelineKey->sType);
    serialize_pnext_chain(payload, pPipelineKey->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineKeyKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetPipelineKeyKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderingAreaGranularityKHR(::rvk::protocol::Transport& t, VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
    RVK_TRACE("client vkGetRenderingAreaGranularityKHR(device={:#x}, pRenderingAreaInfo=ptr, pGranularity=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkRenderingAreaInfo(payload, *pRenderingAreaInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderingAreaGranularityKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetRenderingAreaGranularityKHR -> result_code={}", result_code);
}

VkResult rvk_vkReleaseCapturedPipelineDataKHR(::rvk::protocol::Transport& t, VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) {
    RVK_TRACE("client vkReleaseCapturedPipelineDataKHR(device={:#x}, pInfo=ptr, pAllocator=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkReleaseCapturedPipelineDataInfoKHR(payload, *pInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkReleaseCapturedPipelineDataKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkReleaseCapturedPipelineDataKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQueryIndexedEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    RVK_TRACE("client vkCmdBeginQueryIndexedEXT(commandBuffer={:#x}, queryPool={:#x}, query={}, flags={}, index={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(flags), static_cast<long long>(index));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    payload.write_pod(index);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT, payload.bytes());
    RVK_TRACE("client vkCmdBeginQueryIndexedEXT -> (async, no reply expected)");
}

void rvk_vkCmdBeginTransformFeedbackEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    RVK_TRACE("client vkCmdBeginTransformFeedbackEXT(commandBuffer={:#x}, firstCounterBuffer={}, counterBufferCount={}, pCounterBuffers=ptr, pCounterBufferOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstCounterBuffer), static_cast<long long>(counterBufferCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
        serialize_VkBuffer(payload, pCounterBuffers[__i]);
    }
    const std::uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
            payload.write_pod(pCounterBufferOffsets[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT, payload.bytes());
    RVK_TRACE("client vkCmdBeginTransformFeedbackEXT -> (async, no reply expected)");
}

void rvk_vkCmdBindTransformFeedbackBuffersEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    RVK_TRACE("client vkCmdBindTransformFeedbackBuffersEXT(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        serialize_VkBuffer(payload, pBuffers[__i]);
    }
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
        payload.write_pod(pOffsets[__i]);
    }
    const std::uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            payload.write_pod(pSizes[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT, payload.bytes());
    RVK_TRACE("client vkCmdBindTransformFeedbackBuffersEXT -> (async, no reply expected)");
}

void rvk_vkCmdDrawIndirectByteCountEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    RVK_TRACE("client vkCmdDrawIndirectByteCountEXT(commandBuffer={:#x}, instanceCount={}, firstInstance={}, counterBuffer={:#x}, counterBufferOffset={}, counterOffset={}, vertexStride={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(instanceCount), static_cast<long long>(firstInstance), ::rvk::instr::trace_handle(counterBuffer), static_cast<long long>(counterBufferOffset), static_cast<long long>(counterOffset), static_cast<long long>(vertexStride));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    serialize_VkBuffer(payload, counterBuffer);
    payload.write_pod(counterBufferOffset);
    payload.write_pod(counterOffset);
    payload.write_pod(vertexStride);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT, payload.bytes());
    RVK_TRACE("client vkCmdDrawIndirectByteCountEXT -> (async, no reply expected)");
}

void rvk_vkCmdEndQueryIndexedEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    RVK_TRACE("client vkCmdEndQueryIndexedEXT(commandBuffer={:#x}, queryPool={:#x}, query={}, index={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(index));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(index);
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT, payload.bytes());
    RVK_TRACE("client vkCmdEndQueryIndexedEXT -> (async, no reply expected)");
}

void rvk_vkCmdEndTransformFeedbackEXT(::rvk::protocol::Transport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    RVK_TRACE("client vkCmdEndTransformFeedbackEXT(commandBuffer={:#x}, firstCounterBuffer={}, counterBufferCount={}, pCounterBuffers=ptr, pCounterBufferOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstCounterBuffer), static_cast<long long>(counterBufferCount));
    ::av::WireWriter payload;
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
        serialize_VkBuffer(payload, pCounterBuffers[__i]);
    }
    const std::uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
            payload.write_pod(pCounterBufferOffsets[__i]);
        }
    }
    (void)t.send_async(::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT, payload.bytes());
    RVK_TRACE("client vkCmdEndTransformFeedbackEXT -> (async, no reply expected)");
}

void rvk_vkSetDeviceMemoryPriorityEXT(::rvk::protocol::Transport& t, VkDevice device, VkDeviceMemory memory, float priority) {
    RVK_TRACE("client vkSetDeviceMemoryPriorityEXT(device={:#x}, memory={:#x}, priority={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory), priority);
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(priority);
    (void)t.send_async(::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT, payload.bytes());
    RVK_TRACE("client vkSetDeviceMemoryPriorityEXT -> (async, no reply expected)");
}

VkResult rvk_vkGetFenceFdKHR(::rvk::protocol::Transport& t, VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    RVK_TRACE("client vkGetFenceFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkFenceGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceFdKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetFenceFdKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdKHR(::rvk::protocol::Transport& t, VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    RVK_TRACE("client vkGetMemoryFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetMemoryFdKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdPropertiesKHR(::rvk::protocol::Transport& t, VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    RVK_TRACE("client vkGetMemoryFdPropertiesKHR(device={:#x}, handleType={}, fd={}, pMemoryFdProperties=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(handleType), static_cast<long long>(fd));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    payload.write_pod(handleType);
    payload.write_pod(fd);
    payload.write_pod(pMemoryFdProperties->sType);
    serialize_pnext_chain(payload, pMemoryFdProperties->pNext);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetMemoryFdPropertiesKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetSemaphoreFdKHR(::rvk::protocol::Transport& t, VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    RVK_TRACE("client vkGetSemaphoreFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreGetFdInfoKHR(payload, *pGetFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreFdKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
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
    RVK_TRACE("client vkGetSemaphoreFdKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportFenceFdKHR(::rvk::protocol::Transport& t, VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    RVK_TRACE("client vkImportFenceFdKHR(device={:#x}, pImportFenceFdInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImportFenceFdInfoKHR(payload, *pImportFenceFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkImportFenceFdKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkImportFenceFdKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportSemaphoreFdKHR(::rvk::protocol::Transport& t, VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    RVK_TRACE("client vkImportSemaphoreFdKHR(device={:#x}, pImportSemaphoreFdInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkImportSemaphoreFdInfoKHR(payload, *pImportSemaphoreFdInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkImportSemaphoreFdKHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkImportSemaphoreFdKHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkUnmapMemory2KHR(::rvk::protocol::Transport& t, VkDevice device, const VkMemoryUnmapInfo* pMemoryUnmapInfo) {
    RVK_TRACE("client vkUnmapMemory2KHR(device={:#x}, pMemoryUnmapInfo=ptr)",::rvk::instr::trace_handle(device));
    ::av::WireWriter payload;
    serialize_VkDevice(payload, device);
    serialize_VkMemoryUnmapInfo(payload, *pMemoryUnmapInfo);
    ::av::WireWriter reply;
    if (t.send_sync(::av::vkproto::kCmd_vkUnmapMemory2KHR, payload.bytes(), &reply) != ::rvk::protocol::TransportResult::Ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    ::av::WireReader r{reply.bytes()};
    std::int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    RVK_TRACE("client vkUnmapMemory2KHR -> result_code={}", result_code);
    return static_cast<VkResult>(result_code);
}

}  // namespace av::vkproto
