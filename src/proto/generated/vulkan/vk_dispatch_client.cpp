// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/sync_call_scope.hpp"
#include "proto/wire/wire_writer.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

void rvk_vkDestroyInstance(::av::ClientTransport& t, VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyInstance");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, instance);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyInstance, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyDevice(::av::ClientTransport& t, VkDevice device, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDevice");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDevice, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkGetDeviceQueue(::av::ClientTransport& t, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, uint64_t* pQueue) {
    AV_SCOPE("rvk_vkGetDeviceQueue");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(queueFamilyIndex);
    payload.write_pod(queueIndex);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueue)) {
        return;
    }
}

VkResult rvk_vkEnumeratePhysicalDevices(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceCount, uint64_t* pPhysicalDevices) {
    AV_SCOPE("rvk_vkEnumeratePhysicalDevices");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, instance);
    const uint8_t array_present = pPhysicalDevices != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDevices, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pPhysicalDeviceCount_answered = 0;
    if (!r.read_pod(pPhysicalDeviceCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pPhysicalDeviceCount_answered > *pPhysicalDeviceCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pPhysicalDeviceCount_answered; ++k) {
            if (!r.read_pod(pPhysicalDevices[k])) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPhysicalDeviceCount = pPhysicalDeviceCount_answered;
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceProperties2(r, pProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkCreateInstance(::av::ClientTransport& t, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, uint64_t* pInstance) {
    AV_SCOPE("rvk_vkCreateInstance");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstanceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateInstance, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pInstance)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDevice(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, uint64_t* pDevice) {
    AV_SCOPE("rvk_vkCreateDevice");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkDeviceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDevice, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDevice)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceProperties(r, pProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFeatures(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFeatures");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceFeatures(r, pFeatures, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkFormatProperties(r, pFormatProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceImageFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(tiling);
    payload.write_pod(usage);
    payload.write_pod(flags);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkImageFormatProperties(r, pImageFormatProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceMemoryProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceMemoryProperties(r, pMemoryProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceQueueFamilyProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pQueueFamilyPropertyCount_answered = 0;
    if (!r.read_pod(pQueueFamilyPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pQueueFamilyPropertyCount_answered > *pQueueFamilyPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pQueueFamilyPropertyCount_answered; ++k) {
            if (!deserialize_VkQueueFamilyProperties(r, &pQueueFamilyProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pQueueFamilyPropertyCount = pQueueFamilyPropertyCount_answered;
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceSparseImageFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(samples);
    payload.write_pod(usage);
    payload.write_pod(tiling);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pPropertyCount_answered = 0;
    if (!r.read_pod(pPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pPropertyCount_answered > *pPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pPropertyCount_answered; ++k) {
            if (!deserialize_VkSparseImageFormatProperties(r, &pProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
}

VkResult rvk_vkEnumerateDeviceExtensionProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    AV_SCOPE("rvk_vkEnumerateDeviceExtensionProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    {
        const char* s = pLayerName;
        if (s == nullptr) {
            payload.write_varint(0);
        } else {
            const size_t n = __builtin_strlen(s) + 1u;
            payload.write_varint(n);
            payload.write(s, n);
        }
    }
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceExtensionProperties, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pPropertyCount_answered = 0;
    if (!r.read_pod(pPropertyCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pPropertyCount_answered > *pPropertyCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pPropertyCount_answered; ++k) {
            if (!deserialize_VkExtensionProperties(r, &pProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkEnumerateDeviceLayerProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    AV_SCOPE("rvk_vkEnumerateDeviceLayerProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkEnumerateDeviceLayerProperties, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pPropertyCount_answered = 0;
    if (!r.read_pod(pPropertyCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pPropertyCount_answered > *pPropertyCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pPropertyCount_answered; ++k) {
            if (!deserialize_VkLayerProperties(r, &pProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFeatures2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceFeatures2(r, pFeatures, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkFormatProperties2(r, pFormatProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceImageFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkImageFormatProperties2(r, pImageFormatProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceMemoryProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, pMemoryProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceQueueFamilyProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pQueueFamilyPropertyCount_answered = 0;
    if (!r.read_pod(pQueueFamilyPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pQueueFamilyPropertyCount_answered > *pQueueFamilyPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pQueueFamilyPropertyCount_answered; ++k) {
            if (!deserialize_VkQueueFamilyProperties2(r, &pQueueFamilyProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pQueueFamilyPropertyCount = pQueueFamilyPropertyCount_answered;
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceSparseImageFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pPropertyCount_answered = 0;
    if (!r.read_pod(pPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pPropertyCount_answered > *pPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pPropertyCount_answered; ++k) {
            if (!deserialize_VkSparseImageFormatProperties2(r, &pProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
}

VkResult rvk_vkDeviceWaitIdle(::av::ClientTransport& t, VkDevice device) {
    AV_SCOPE("rvk_vkDeviceWaitIdle");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkDeviceWaitIdle, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueWaitIdle(::av::ClientTransport& t, VkQueue queue) {
    AV_SCOPE("rvk_vkQueueWaitIdle");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, queue);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkQueueWaitIdle, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateMemory(::av::ClientTransport& t, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    AV_SCOPE("rvk_vkAllocateMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkMemoryAllocateInfo(payload, *pAllocateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateMemory, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pMemory)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkFreeMemory(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkFreeMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkFreeMemory, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkBindBufferMemory(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("rvk_vkBindBufferMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory(::av::ClientTransport& t, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("rvk_vkBindImageMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(memoryOffset);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetBufferMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkCreateBuffer(::av::ClientTransport& t, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    AV_SCOPE("rvk_vkCreateBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBuffer, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pBuffer)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBuffer(::av::ClientTransport& t, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBuffer(payload, buffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateImage(::av::ClientTransport& t, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    AV_SCOPE("rvk_vkCreateImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImage, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pImage)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImage(::av::ClientTransport& t, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateImageView(::av::ClientTransport& t, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    AV_SCOPE("rvk_vkCreateImageView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateImageView, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pView)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyImageView(::av::ClientTransport& t, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyImageView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageView(payload, imageView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyImageView, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateSampler(::av::ClientTransport& t, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    AV_SCOPE("rvk_vkCreateSampler");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSamplerCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSampler, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSampler)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySampler(::av::ClientTransport& t, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroySampler");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSampler(payload, sampler);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySampler, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateSemaphore(::av::ClientTransport& t, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    AV_SCOPE("rvk_vkCreateSemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSemaphore, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSemaphore)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroySemaphore(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroySemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySemaphore, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateFence(::av::ClientTransport& t, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    AV_SCOPE("rvk_vkCreateFence");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFenceCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFence, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFence)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFence(::av::ClientTransport& t, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyFence");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyFence, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkResetFences(::av::ClientTransport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    AV_SCOPE("rvk_vkResetFences");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(fenceCount); ++rvk_i) {
        serialize_VkFence(payload, pFences[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkResetFences, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult rvk_vkWaitForFences(::av::ClientTransport& t, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    AV_SCOPE("rvk_vkWaitForFences");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(fenceCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(fenceCount); ++rvk_i) {
        serialize_VkFence(payload, pFences[rvk_i]);
    }
    payload.write_pod(waitAll);
    payload.write_pod(timeout);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkWaitForFences, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetFenceStatus(::av::ClientTransport& t, VkDevice device, VkFence fence) {
    AV_SCOPE("rvk_vkGetFenceStatus");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFence(payload, fence);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceStatus, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateCommandPool(::av::ClientTransport& t, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    AV_SCOPE("rvk_vkCreateCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateCommandPool, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pCommandPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyCommandPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkResetCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    AV_SCOPE("rvk_vkResetCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkResetCommandPool, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult rvk_vkBeginCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    AV_SCOPE("rvk_vkBeginCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCommandBufferBeginInfo(payload, *pBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkBeginCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult rvk_vkEndCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    AV_SCOPE("rvk_vkEndCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkEndCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult rvk_vkResetCommandBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    AV_SCOPE("rvk_vkResetCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkResetCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void rvk_vkDestroyShaderModule(::av::ClientTransport& t, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyShaderModule");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkShaderModule(payload, shaderModule);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyShaderModule, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateShaderModule(::av::ClientTransport& t, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    AV_SCOPE("rvk_vkCreateShaderModule");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkShaderModuleCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShaderModule, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pShaderModule)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePipelineLayout(::av::ClientTransport& t, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    AV_SCOPE("rvk_vkCreatePipelineLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineLayout, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPipelineLayout)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineLayout(::av::ClientTransport& t, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyPipelineLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineLayout(payload, pipelineLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineLayout, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyPipeline(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyPipeline");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipeline, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreatePipelineCache(::av::ClientTransport& t, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    AV_SCOPE("rvk_vkCreatePipelineCache");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCacheCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineCache, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPipelineCache)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineCache(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyPipelineCache");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineCache, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateRenderPass(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("rvk_vkCreateRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyRenderPass(::av::ClientTransport& t, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateFramebuffer(::av::ClientTransport& t, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    AV_SCOPE("rvk_vkCreateFramebuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFramebufferCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateFramebuffer, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFramebuffer)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyFramebuffer(::av::ClientTransport& t, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyFramebuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFramebuffer(payload, framebuffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyFramebuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateDescriptorSetLayout(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    AV_SCOPE("rvk_vkCreateDescriptorSetLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorSetLayout, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pSetLayout)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorSetLayout(::av::ClientTransport& t, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDescriptorSetLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayout(payload, descriptorSetLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorSetLayout, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateDescriptorPool(::av::ClientTransport& t, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    AV_SCOPE("rvk_vkCreateDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorPool, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorPool(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkFreeCommandBuffers(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("rvk_vkFreeCommandBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(commandBufferCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(commandBufferCount); ++rvk_i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkFreeCommandBuffers, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkQueueSubmit(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    AV_SCOPE("rvk_vkQueueSubmit");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    if (t.send_async(::av::vkproto::kCmd_vkQueueSubmit, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void rvk_vkCmdBindPipeline(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    AV_SCOPE("rvk_vkCmdBindPipeline");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipeline(payload, pipeline);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindPipeline, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetViewport(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("rvk_vkCmdSetViewport");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewport, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetScissor(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("rvk_vkCmdSetScissor");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstScissor);
    payload.write_pod(scissorCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissor, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindVertexBuffers(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    AV_SCOPE("rvk_vkCmdBindVertexBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindIndexBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    AV_SCOPE("rvk_vkCmdBindIndexBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(indexType);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindDescriptorSets(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    AV_SCOPE("rvk_vkCmdBindDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(firstSet);
    payload.write_pod(descriptorSetCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(descriptorSetCount); ++rvk_i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[rvk_i]);
    }
    payload.write_pod(dynamicOffsetCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(dynamicOffsetCount); ++rvk_i) {
        payload.write_pod(pDynamicOffsets[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorSets, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDraw(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    AV_SCOPE("rvk_vkCmdDraw");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstVertex);
    payload.write_pod(firstInstance);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDraw, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndexed(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    AV_SCOPE("rvk_vkCmdDrawIndexed");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(indexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstIndex);
    payload.write_pod(vertexOffset);
    payload.write_pod(firstInstance);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexed, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPipelineBarrier(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("rvk_vkCmdPipelineBarrier");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(dependencyFlags);
    payload.write_pod(memoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(memoryBarrierCount); ++rvk_i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[rvk_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bufferMemoryBarrierCount); ++rvk_i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[rvk_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(imageMemoryBarrierCount); ++rvk_i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    AV_SCOPE("rvk_vkCmdCopyBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferCopy(payload, pRegions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBeginRenderPass(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    AV_SCOPE("rvk_vkCmdBeginRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    payload.write_pod(contents);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndRenderPass(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    AV_SCOPE("rvk_vkCmdEndRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdNextSubpass(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    AV_SCOPE("rvk_vkCmdNextSubpass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(contents);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPushConstants(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    AV_SCOPE("rvk_vkCmdPushConstants");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(stageFlags);
    payload.write_pod(offset);
    payload.write_pod(size);
    if (pValues != nullptr && size > 0) {
        payload.write(pValues, static_cast<size_t>(size));
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushConstants, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateGraphicsPipelines(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("rvk_vkCreateGraphicsPipelines");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkGraphicsPipelineCreateInfo(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateGraphicsPipelines, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkUpdateDescriptorSets(::av::ClientTransport& t, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    AV_SCOPE("rvk_vkUpdateDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(descriptorWriteCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(descriptorWriteCount); ++rvk_i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[rvk_i]);
    }
    payload.write_pod(descriptorCopyCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(descriptorCopyCount); ++rvk_i) {
        serialize_VkCopyDescriptorSet(payload, pDescriptorCopies[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSets, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkAllocateCommandBuffers(::av::ClientTransport& t, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, uint64_t* pCommandBuffers) {
    AV_SCOPE("rvk_vkAllocateCommandBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandBufferAllocateInfo(payload, *pAllocateInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateCommandBuffers, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(pAllocateInfo->commandBufferCount); ++rvk_k) {
        if (!r.read_pod(pCommandBuffers[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkAllocateDescriptorSets(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("rvk_vkAllocateDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetAllocateInfo(payload, *pAllocateInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateDescriptorSets, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(pAllocateInfo->descriptorSetCount); ++rvk_k) {
        if (!r.read_pod(pDescriptorSets[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFreeDescriptorSets(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("rvk_vkFreeDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(descriptorSetCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(descriptorSetCount); ++rvk_i) {
        serialize_VkDescriptorSet(payload, pDescriptorSets[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkFreeDescriptorSets, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateComputePipelines(::av::ClientTransport& t, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("rvk_vkCreateComputePipelines");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkComputePipelineCreateInfo(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateComputePipelines, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetDescriptorPool(::av::ClientTransport& t, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    AV_SCOPE("rvk_vkResetDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorPool(payload, descriptorPool);
    payload.write_pod(flags);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkResetDescriptorPool, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateEvent(::av::ClientTransport& t, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    AV_SCOPE("rvk_vkCreateEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkEventCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateEvent, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pEvent)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyEvent(::av::ClientTransport& t, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkGetEventStatus(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    AV_SCOPE("rvk_vkGetEventStatus");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetEventStatus, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSetEvent(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    AV_SCOPE("rvk_vkSetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkSetEvent, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkResetEvent(::av::ClientTransport& t, VkDevice device, VkEvent event) {
    AV_SCOPE("rvk_vkResetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkEvent(payload, event);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkResetEvent, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetEvent(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("rvk_vkCmdSetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResetEvent(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("rvk_vkCmdResetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWaitEvents(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("rvk_vkCmdWaitEvents");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(memoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(memoryBarrierCount); ++rvk_i) {
        serialize_VkMemoryBarrier(payload, pMemoryBarriers[rvk_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bufferMemoryBarrierCount); ++rvk_i) {
        serialize_VkBufferMemoryBarrier(payload, pBufferMemoryBarriers[rvk_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(imageMemoryBarrierCount); ++rvk_i) {
        serialize_VkImageMemoryBarrier(payload, pImageMemoryBarriers[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    AV_SCOPE("rvk_vkCmdCopyImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageCopy(payload, pRegions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBufferToImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("rvk_vkCmdCopyBufferToImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, srcBuffer);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferImageCopy(payload, pRegions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImageToBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("rvk_vkCmdCopyImageToBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkBufferImageCopy(payload, pRegions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBlitImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    AV_SCOPE("rvk_vkCmdBlitImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageBlit(payload, pRegions[rvk_i]);
    }
    payload.write_pod(filter);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdClearColorImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("rvk_vkCmdClearColorImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearColorValue(payload, *pColor);
    payload.write_pod(rangeCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(rangeCount); ++rvk_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearColorImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdClearDepthStencilImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("rvk_vkCmdClearDepthStencilImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, image);
    payload.write_pod(imageLayout);
    serialize_VkClearDepthStencilValue(payload, *pDepthStencil);
    payload.write_pod(rangeCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(rangeCount); ++rvk_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearDepthStencilImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdClearAttachments(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    AV_SCOPE("rvk_vkCmdClearAttachments");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkClearAttachment(payload, pAttachments[rvk_i]);
    }
    payload.write_pod(rectCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(rectCount); ++rvk_i) {
        serialize_VkClearRect(payload, pRects[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearAttachments, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdFillBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    AV_SCOPE("rvk_vkCmdFillBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(size);
    payload.write_pod(data);
    if (t.send_async(::av::vkproto::kCmd_vkCmdFillBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdUpdateBuffer(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    AV_SCOPE("rvk_vkCmdUpdateBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(dataSize);
    if (pData != nullptr && dataSize > 0) {
        payload.write(pData, static_cast<size_t>(dataSize));
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdUpdateBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLineWidth(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float lineWidth) {
    AV_SCOPE("rvk_vkCmdSetLineWidth");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineWidth);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineWidth, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBias(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    AV_SCOPE("rvk_vkCmdSetDepthBias");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasConstantFactor);
    payload.write_pod(depthBiasClamp);
    payload.write_pod(depthBiasSlopeFactor);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBias, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetBlendConstants(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const float* blendConstants) {
    AV_SCOPE("rvk_vkCmdSetBlendConstants");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    for (uint32_t rvk_i = 0; rvk_i < 4u; ++rvk_i) {
        payload.write_pod(blendConstants[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetBlendConstants, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBounds(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    AV_SCOPE("rvk_vkCmdSetDepthBounds");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(minDepthBounds);
    payload.write_pod(maxDepthBounds);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBounds, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilCompareMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    AV_SCOPE("rvk_vkCmdSetStencilCompareMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(compareMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilCompareMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilWriteMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    AV_SCOPE("rvk_vkCmdSetStencilWriteMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(writeMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilWriteMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilReference(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    AV_SCOPE("rvk_vkCmdSetStencilReference");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(reference);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilReference, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDispatch(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("rvk_vkCmdDispatch");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatch, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDispatchIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    AV_SCOPE("rvk_vkCmdDispatchIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatchIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndexedIndirect(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndexedIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdExecuteCommands(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("rvk_vkCmdExecuteCommands");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(commandBufferCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(commandBufferCount); ++rvk_i) {
        serialize_VkCommandBuffer(payload, pCommandBuffers[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdExecuteCommands, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkAcquireNextImage2KHR(::av::ClientTransport& t, VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    AV_SCOPE("rvk_vkAcquireNextImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkAcquireNextImageInfoKHR(payload, *pAcquireInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAcquireNextImage2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pImageIndex)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindBufferMemory2(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("rvk_vkBindBufferMemory2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("rvk_vkBindImageMemory2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQuery(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    AV_SCOPE("rvk_vkCmdBeginQuery");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginQuery, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBeginRenderPass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("rvk_vkCmdBeginRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBeginRendering(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("rvk_vkCmdBeginRendering");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRendering, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindVertexBuffers2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("rvk_vkCmdBindVertexBuffers2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    const uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pStrides[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBlitImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("rvk_vkCmdBlitImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBuffer2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("rvk_vkCmdCopyBuffer2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBufferToImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("rvk_vkCmdCopyBufferToImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("rvk_vkCmdCopyImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImageToBuffer2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("rvk_vkCmdCopyImageToBuffer2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyQueryPoolResults(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("rvk_vkCmdCopyQueryPoolResults");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    serialize_VkBuffer(payload, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(stride);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyQueryPoolResults, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDispatchBase(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("rvk_vkCmdDispatchBase");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatchBase, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndexedIndirectCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndexedIndirectCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndirectCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndirectCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndQuery(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("rvk_vkCmdEndQuery");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndQuery, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndRenderPass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("rvk_vkCmdEndRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndRendering(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    AV_SCOPE("rvk_vkCmdEndRendering");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRendering, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdNextSubpass2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("rvk_vkCmdNextSubpass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPipelineBarrier2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("rvk_vkCmdPipelineBarrier2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResetEvent2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("rvk_vkCmdResetEvent2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResetQueryPool(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("rvk_vkCmdResetQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResolveImage(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    AV_SCOPE("rvk_vkCmdResolveImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkImage(payload, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(regionCount); ++rvk_i) {
        serialize_VkImageResolve(payload, pRegions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResolveImage2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("rvk_vkCmdResolveImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCullMode(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("rvk_vkCmdSetCullMode");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCullMode, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBiasEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthBiasEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBoundsTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthBoundsTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthCompareOp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("rvk_vkCmdSetDepthCompareOp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthWriteEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthWriteEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDeviceMask(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("rvk_vkCmdSetDeviceMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetEvent2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("rvk_vkCmdSetEvent2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetFrontFace(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("rvk_vkCmdSetFrontFace");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFace, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPrimitiveRestartEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("rvk_vkCmdSetPrimitiveRestartEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPrimitiveTopology(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("rvk_vkCmdSetPrimitiveTopology");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopology, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRasterizerDiscardEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("rvk_vkCmdSetRasterizerDiscardEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetScissorWithCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("rvk_vkCmdSetScissorWithCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilOp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("rvk_vkCmdSetStencilOp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilTestEnable(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("rvk_vkCmdSetStencilTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetViewportWithCount(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("rvk_vkCmdSetViewportWithCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWaitEvents2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("rvk_vkCmdWaitEvents2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(eventCount); ++rvk_i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWriteTimestamp(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("rvk_vkCmdWriteTimestamp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWriteTimestamp2(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("rvk_vkCmdWriteTimestamp2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateBufferView(::av::ClientTransport& t, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    AV_SCOPE("rvk_vkCreateBufferView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferViewCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateBufferView, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pView)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDescriptorUpdateTemplate(::av::ClientTransport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("rvk_vkCreateDescriptorUpdateTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplate, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorUpdateTemplate)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreatePrivateDataSlot(::av::ClientTransport& t, VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    AV_SCOPE("rvk_vkCreatePrivateDataSlot");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlotCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePrivateDataSlot, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pPrivateDataSlot)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateQueryPool(::av::ClientTransport& t, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    AV_SCOPE("rvk_vkCreateQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkQueryPoolCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateQueryPool, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pQueryPool)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("rvk_vkCreateRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversion(::av::ClientTransport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("rvk_vkCreateSamplerYcbcrConversion");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversion, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pYcbcrConversion)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyBufferView(::av::ClientTransport& t, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyBufferView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferView(payload, bufferView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyBufferView, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyDescriptorUpdateTemplate(::av::ClientTransport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDescriptorUpdateTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyPrivateDataSlot(::av::ClientTransport& t, VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyPrivateDataSlot");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPrivateDataSlot, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyQueryPool(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroySamplerYcbcrConversion(::av::ClientTransport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroySamplerYcbcrConversion");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkEnumeratePhysicalDeviceGroups(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("rvk_vkEnumeratePhysicalDeviceGroups");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, instance);
    const uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceGroupCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroups, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pPhysicalDeviceGroupCount_answered = 0;
    if (!r.read_pod(pPhysicalDeviceGroupCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pPhysicalDeviceGroupCount_answered > *pPhysicalDeviceGroupCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pPhysicalDeviceGroupCount_answered; ++k) {
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, &pPhysicalDeviceGroupProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkFlushMappedMemoryRanges(::av::ClientTransport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("rvk_vkFlushMappedMemoryRanges");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(memoryRangeCount); ++rvk_i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkFlushMappedMemoryRanges, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddress(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetBufferDeviceAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddress, payload, r) != ::av::TransportResult::ok) {
        return static_cast<VkDeviceAddress>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<VkDeviceAddress>(0);
    }
    return static_cast<VkDeviceAddress>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetBufferMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddress(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetBufferOpaqueCaptureAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddress, payload, r) != ::av::TransportResult::ok) {
        return static_cast<uint64_t>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<uint64_t>(0);
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupport(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("rvk_vkGetDescriptorSetLayoutSupport");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkDescriptorSetLayoutSupport(r, pSupport, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceBufferMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceBufferMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceGroupPeerMemoryFeatures(::av::ClientTransport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("rvk_vkGetDeviceGroupPeerMemoryFeatures");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeatures, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPeerMemoryFeatures)) {
        return;
    }
}

VkResult rvk_vkGetDeviceGroupPresentCapabilitiesKHR(::av::ClientTransport& t, VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    AV_SCOPE("rvk_vkGetDeviceGroupPresentCapabilitiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(pDeviceGroupPresentCapabilities->sType);
    serialize_pnext_chain(payload, pDeviceGroupPresentCapabilities->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkDeviceGroupPresentCapabilitiesKHR(r, pDeviceGroupPresentCapabilities, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetDeviceGroupSurfacePresentModesKHR(::av::ClientTransport& t, VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    AV_SCOPE("rvk_vkGetDeviceGroupSurfacePresentModesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSurfaceKHR(payload, surface);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupSurfacePresentModesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pModes)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceImageMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceImageMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceImageSparseMemoryRequirements(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceImageSparseMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pSparseMemoryRequirementCount_answered = 0;
    if (!r.read_pod(pSparseMemoryRequirementCount_answered)) {
        return;
    }
    if (array_present) {
        if (pSparseMemoryRequirementCount_answered > *pSparseMemoryRequirementCount) {
            return;
        }
        for (uint32_t k = 0; k < pSparseMemoryRequirementCount_answered; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void rvk_vkGetDeviceMemoryCommitment(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    AV_SCOPE("rvk_vkGetDeviceMemoryCommitment");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryCommitment, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pCommittedMemoryInBytes)) {
        return;
    }
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddress(::av::ClientTransport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetDeviceMemoryOpaqueCaptureAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress, payload, r) != ::av::TransportResult::ok) {
        return static_cast<uint64_t>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<uint64_t>(0);
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDeviceQueue2(::av::ClientTransport& t, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, uint64_t* pQueue) {
    AV_SCOPE("rvk_vkGetDeviceQueue2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceQueueInfo2(payload, *pQueueInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceQueue2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pQueue)) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetImageSparseMemoryRequirements(::av::ClientTransport& t, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageSparseMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pSparseMemoryRequirementCount_answered = 0;
    if (!r.read_pod(pSparseMemoryRequirementCount_answered)) {
        return;
    }
    if (array_present) {
        if (pSparseMemoryRequirementCount_answered > *pSparseMemoryRequirementCount) {
            return;
        }
        for (uint32_t k = 0; k < pSparseMemoryRequirementCount_answered; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements(r, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void rvk_vkGetImageSparseMemoryRequirements2(::av::ClientTransport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageSparseMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pSparseMemoryRequirementCount_answered = 0;
    if (!r.read_pod(pSparseMemoryRequirementCount_answered)) {
        return;
    }
    if (array_present) {
        if (pSparseMemoryRequirementCount_answered > *pSparseMemoryRequirementCount) {
            return;
        }
        for (uint32_t k = 0; k < pSparseMemoryRequirementCount_answered; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void rvk_vkGetImageSubresourceLayout(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    AV_SCOPE("rvk_vkGetImageSubresourceLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource(payload, *pSubresource);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout(r, pLayout, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalBufferProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalBufferProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalBufferProperties(r, pExternalBufferProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalFenceProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalFenceProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalFenceProperties(r, pExternalFenceProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalSemaphoreProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalSemaphoreProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalSemaphoreProperties(r, pExternalSemaphoreProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDevicePresentRectanglesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    AV_SCOPE("rvk_vkGetPhysicalDevicePresentRectanglesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkSurfaceKHR(payload, surface);
    const uint8_t array_present = pRects != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pRectCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDevicePresentRectanglesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pRectCount_answered = 0;
    if (!r.read_pod(pRectCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pRectCount_answered > *pRectCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pRectCount_answered; ++k) {
            if (!deserialize_VkRect2D(r, &pRects[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pRectCount = pRectCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetPhysicalDeviceToolProperties(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceToolProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pToolProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pToolCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceToolProperties, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pToolCount_answered = 0;
    if (!r.read_pod(pToolCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pToolCount_answered > *pToolCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pToolCount_answered; ++k) {
            if (!deserialize_VkPhysicalDeviceToolProperties(r, &pToolProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pToolCount = pToolCount_answered;
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPrivateData(::av::ClientTransport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    AV_SCOPE("rvk_vkGetPrivateData");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPrivateData, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pData)) {
        return;
    }
}

VkResult rvk_vkGetQueryPoolResults(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("rvk_vkGetQueryPoolResults");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    payload.write_pod<uint64_t>(dataSize);
    payload.write_pod(stride);
    payload.write_pod(flags);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetQueryPoolResults, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderAreaGranularity(::av::ClientTransport& t, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    AV_SCOPE("rvk_vkGetRenderAreaGranularity");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderPass(payload, renderPass);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderAreaGranularity, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExtent2D(r, pGranularity, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetSemaphoreCounterValue(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("rvk_vkGetSemaphoreCounterValue");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValue, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pValue)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkInvalidateMappedMemoryRanges(::av::ClientTransport& t, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("rvk_vkInvalidateMappedMemoryRanges");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(memoryRangeCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(memoryRangeCount); ++rvk_i) {
        serialize_VkMappedMemoryRange(payload, pMemoryRanges[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkInvalidateMappedMemoryRanges, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkMergePipelineCaches(::av::ClientTransport& t, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    AV_SCOPE("rvk_vkMergePipelineCaches");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCache(payload, dstCache);
    payload.write_pod(srcCacheCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(srcCacheCount); ++rvk_i) {
        serialize_VkPipelineCache(payload, pSrcCaches[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkMergePipelineCaches, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueBindSparse(::av::ClientTransport& t, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    AV_SCOPE("rvk_vkQueueBindSparse");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, queue);
    payload.write_pod(bindInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindSparseInfo(payload, pBindInfo[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkQueueBindSparse, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("rvk_vkQueueSubmit2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo2(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    if (t.send_async(::av::vkproto::kCmd_vkQueueSubmit2, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void rvk_vkResetQueryPool(::av::ClientTransport& t, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("rvk_vkResetQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    if (t.send_async(::av::vkproto::kCmd_vkResetQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkSetPrivateData(::av::ClientTransport& t, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    AV_SCOPE("rvk_vkSetPrivateData");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, privateDataSlot);
    payload.write_pod(data);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkSetPrivateData, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphore(::av::ClientTransport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("rvk_vkSignalSemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphore, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPool(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("rvk_vkTrimCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkTrimCommandPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkUpdateDescriptorSetWithTemplate(::av::ClientTransport& t, VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("rvk_vkUpdateDescriptorSetWithTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSet(payload, descriptorSet);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    const uint64_t rvk_blob = (pData != nullptr) ? descriptor_template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(rvk_blob);
    if (rvk_blob != 0) { payload.write(pData, static_cast<size_t>(rvk_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplate, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkWaitSemaphores(::av::ClientTransport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("rvk_vkWaitSemaphores");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphores, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceFeatures2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFeatures2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, pFeatures->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceFeatures2(r, pFeatures, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, pFormatProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkFormatProperties2(r, pFormatProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceImageFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceImageFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, pImageFormatProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkImageFormatProperties2(r, pImageFormatProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetPhysicalDeviceMemoryProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceMemoryProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, pMemoryProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, pMemoryProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, pProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceProperties2(r, pProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceQueueFamilyProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pQueueFamilyPropertyCount_answered = 0;
    if (!r.read_pod(pQueueFamilyPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pQueueFamilyPropertyCount_answered > *pQueueFamilyPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pQueueFamilyPropertyCount_answered; ++k) {
            if (!deserialize_VkQueueFamilyProperties2(r, &pQueueFamilyProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pQueueFamilyPropertyCount = pQueueFamilyPropertyCount_answered;
}

void rvk_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, *pFormatInfo);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pPropertyCount_answered = 0;
    if (!r.read_pod(pPropertyCount_answered)) {
        return;
    }
    if (array_present) {
        if (pPropertyCount_answered > *pPropertyCount) {
            return;
        }
        for (uint32_t k = 0; k < pPropertyCount_answered; ++k) {
            if (!deserialize_VkSparseImageFormatProperties2(r, &pProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
}

VkResult rvk_vkBindBufferMemory2KHR(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("rvk_vkBindBufferMemory2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindBufferMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindBufferMemory2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkBindImageMemory2KHR(::av::ClientTransport& t, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("rvk_vkBindImageMemory2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindInfoCount); ++rvk_i) {
        serialize_VkBindImageMemoryInfo(payload, pBindInfos[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBindImageMemory2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetBufferMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetBufferMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetImageMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetImageSparseMemoryRequirements2KHR(::av::ClientTransport& t, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("rvk_vkGetImageSparseMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pSparseMemoryRequirementCount_answered = 0;
    if (!r.read_pod(pSparseMemoryRequirementCount_answered)) {
        return;
    }
    if (array_present) {
        if (pSparseMemoryRequirementCount_answered > *pSparseMemoryRequirementCount) {
            return;
        }
        for (uint32_t k = 0; k < pSparseMemoryRequirementCount_answered; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void rvk_vkCmdBeginRenderPass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("rvk_vkCmdBeginRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBeginRenderingKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("rvk_vkCmdBeginRenderingKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInfo(payload, *pRenderingInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderingKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBlitImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("rvk_vkCmdBlitImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBlitImageInfo2(payload, *pBlitImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("rvk_vkCmdCopyBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, *pCopyBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyBufferToImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("rvk_vkCmdCopyBufferToImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, *pCopyBufferToImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("rvk_vkCmdCopyImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageInfo2(payload, *pCopyImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyImageToBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("rvk_vkCmdCopyImageToBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, *pCopyImageToBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDispatchBaseKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("rvk_vkCmdDispatchBaseKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(baseGroupX);
    payload.write_pod(baseGroupY);
    payload.write_pod(baseGroupZ);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatchBaseKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndexedIndirectCountKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndexedIndirectCountKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndirectCountKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawIndirectCountKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndRenderPass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("rvk_vkCmdEndRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndRenderingKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    AV_SCOPE("rvk_vkCmdEndRenderingKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderingKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdNextSubpass2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("rvk_vkCmdNextSubpass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPipelineBarrier2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("rvk_vkCmdPipelineBarrier2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPushDescriptorSetWithTemplateKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
    AV_SCOPE("rvk_vkCmdPushDescriptorSetWithTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(set);
    const uint64_t rvk_blob = (pData != nullptr) ? descriptor_template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(rvk_blob);
    if (rvk_blob != 0) { payload.write(pData, static_cast<size_t>(rvk_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetWithTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResetEvent2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("rvk_vkCmdResetEvent2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdResolveImage2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("rvk_vkCmdResolveImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkResolveImageInfo2(payload, *pResolveImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDeviceMaskKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("rvk_vkCmdSetDeviceMaskKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(deviceMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetEvent2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("rvk_vkCmdSetEvent2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkEvent(payload, event);
    serialize_VkDependencyInfo(payload, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWaitEvents2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("rvk_vkCmdWaitEvents2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(eventCount); ++rvk_i) {
        serialize_VkEvent(payload, pEvents[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(eventCount); ++rvk_i) {
        serialize_VkDependencyInfo(payload, pDependencyInfos[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWriteTimestamp2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("rvk_vkCmdWriteTimestamp2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreateDescriptorUpdateTemplateKHR(::av::ClientTransport& t, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("rvk_vkCreateDescriptorUpdateTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplateKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDescriptorUpdateTemplate)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRenderPass2KHR(::av::ClientTransport& t, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("rvk_vkCreateRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderPassCreateInfo2(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRenderPass2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pRenderPass)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateSamplerYcbcrConversionKHR(::av::ClientTransport& t, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("rvk_vkCreateSamplerYcbcrConversionKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateSamplerYcbcrConversionKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pYcbcrConversion)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyDescriptorUpdateTemplateKHR(::av::ClientTransport& t, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDescriptorUpdateTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroySamplerYcbcrConversionKHR(::av::ClientTransport& t, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroySamplerYcbcrConversionKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSamplerYcbcrConversion(payload, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkEnumeratePhysicalDeviceGroupsKHR(::av::ClientTransport& t, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("rvk_vkEnumeratePhysicalDeviceGroupsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, instance);
    const uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceGroupCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroupsKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pPhysicalDeviceGroupCount_answered = 0;
    if (!r.read_pod(pPhysicalDeviceGroupCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pPhysicalDeviceGroupCount_answered > *pPhysicalDeviceGroupCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pPhysicalDeviceGroupCount_answered; ++k) {
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, &pPhysicalDeviceGroupProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount_answered;
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress rvk_vkGetBufferDeviceAddressKHR(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetBufferDeviceAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferDeviceAddressKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<VkDeviceAddress>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<VkDeviceAddress>(0);
    }
    return static_cast<VkDeviceAddress>(result_code);
}

uint64_t rvk_vkGetBufferOpaqueCaptureAddressKHR(::av::ClientTransport& t, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetBufferOpaqueCaptureAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkBufferDeviceAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<uint64_t>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<uint64_t>(0);
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetDescriptorSetLayoutSupportKHR(::av::ClientTransport& t, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("rvk_vkGetDescriptorSetLayoutSupportKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, pSupport->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkDescriptorSetLayoutSupport(r, pSupport, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceBufferMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceBufferMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceGroupPeerMemoryFeaturesKHR(::av::ClientTransport& t, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("rvk_vkGetDeviceGroupPeerMemoryFeaturesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeaturesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pPeerMemoryFeatures)) {
        return;
    }
}

void rvk_vkGetDeviceImageMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceImageMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, pMemoryRequirements->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetDeviceImageSparseMemoryRequirementsKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("rvk_vkGetDeviceImageSparseMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageMemoryRequirements(payload, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirementsKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    uint32_t pSparseMemoryRequirementCount_answered = 0;
    if (!r.read_pod(pSparseMemoryRequirementCount_answered)) {
        return;
    }
    if (array_present) {
        if (pSparseMemoryRequirementCount_answered > *pSparseMemoryRequirementCount) {
            return;
        }
        for (uint32_t k = 0; k < pSparseMemoryRequirementCount_answered; ++k) {
            if (!deserialize_VkSparseImageMemoryRequirements2(r, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

uint64_t rvk_vkGetDeviceMemoryOpaqueCaptureAddressKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("rvk_vkGetDeviceMemoryOpaqueCaptureAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<uint64_t>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<uint64_t>(0);
    }
    return static_cast<uint64_t>(result_code);
}

void rvk_vkGetPhysicalDeviceExternalBufferPropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalBufferPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, pExternalBufferProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalBufferProperties(r, pExternalBufferProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalFencePropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalFencePropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, pExternalFenceProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalFenceProperties(r, pExternalFenceProperties, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, pExternalSemaphoreProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalSemaphoreProperties(r, pExternalSemaphoreProperties, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetSemaphoreCounterValueKHR(::av::ClientTransport& t, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("rvk_vkGetSemaphoreCounterValueKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphore(payload, semaphore);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreCounterValueKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pValue)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkQueueSubmit2KHR(::av::ClientTransport& t, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("rvk_vkQueueSubmit2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, queue);
    payload.write_pod(submitCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(submitCount); ++rvk_i) {
        serialize_VkSubmitInfo2(payload, pSubmits[rvk_i]);
    }
    serialize_VkFence(payload, fence);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkQueueSubmit2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkSignalSemaphoreKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("rvk_vkSignalSemaphoreKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreSignalInfo(payload, *pSignalInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkSignalSemaphoreKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkTrimCommandPoolKHR(::av::ClientTransport& t, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("rvk_vkTrimCommandPoolKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCommandPool(payload, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkTrimCommandPoolKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkUpdateDescriptorSetWithTemplateKHR(::av::ClientTransport& t, VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("rvk_vkUpdateDescriptorSetWithTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDescriptorSet(payload, descriptorSet);
    serialize_VkDescriptorUpdateTemplate(payload, descriptorUpdateTemplate);
    const uint64_t rvk_blob = (pData != nullptr) ? descriptor_template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(rvk_blob);
    if (rvk_blob != 0) { payload.write(pData, static_cast<size_t>(rvk_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkWaitSemaphoresKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("rvk_vkWaitSemaphoresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreWaitInfo(payload, *pWaitInfo);
    payload.write_pod(timeout);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkWaitSemaphoresKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginConditionalRenderingEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    AV_SCOPE("rvk_vkCmdBeginConditionalRenderingEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkConditionalRenderingBeginInfoEXT(payload, *pConditionalRenderingBegin);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndConditionalRenderingEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer) {
    AV_SCOPE("rvk_vkCmdEndConditionalRenderingEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetColorWriteEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    AV_SCOPE("rvk_vkCmdSetColorWriteEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorWriteEnables[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBiasEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthBiasEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBiasEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLogicOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    AV_SCOPE("rvk_vkCmdSetLogicOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPatchControlPointsEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    AV_SCOPE("rvk_vkCmdSetPatchControlPointsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(patchControlPoints);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPrimitiveRestartEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("rvk_vkCmdSetPrimitiveRestartEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRasterizerDiscardEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("rvk_vkCmdSetRasterizerDiscardEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetVertexInputEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    AV_SCOPE("rvk_vkCmdSetVertexInputEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(vertexBindingDescriptionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(vertexBindingDescriptionCount); ++rvk_i) {
        serialize_VkVertexInputBindingDescription2EXT(payload, pVertexBindingDescriptions[rvk_i]);
    }
    payload.write_pod(vertexAttributeDescriptionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(vertexAttributeDescriptionCount); ++rvk_i) {
        serialize_VkVertexInputAttributeDescription2EXT(payload, pVertexAttributeDescriptions[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetVertexInputEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkBuildAccelerationStructuresKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("rvk_vkBuildAccelerationStructuresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    payload.write_pod(infoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        const uint32_t rvk_n = static_cast<uint32_t>(pInfos[rvk_i].geometryCount);
        for (uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[rvk_i][rvk_j]);
        }
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkBuildAccelerationStructuresKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBindShadersEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
    AV_SCOPE("rvk_vkCmdBindShadersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stageCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(stageCount); ++rvk_i) {
        payload.write_pod(pStages[rvk_i]);
    }
    const uint8_t pShaders_present = pShaders != nullptr ? 1 : 0;
    payload.write_pod(pShaders_present);
    if (pShaders_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(stageCount); ++rvk_i) {
            serialize_VkShaderEXT(payload, pShaders[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindShadersEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindVertexBuffers2EXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("rvk_vkCmdBindVertexBuffers2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    const uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pStrides[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBuildAccelerationStructuresIndirectKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) {
    AV_SCOPE("rvk_vkCmdBuildAccelerationStructuresIndirectKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        payload.write_pod(pIndirectDeviceAddresses[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        payload.write_pod(pIndirectStrides[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        const uint32_t rvk_n = static_cast<uint32_t>(pInfos[rvk_i].geometryCount);
        for (uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            payload.write_pod(ppMaxPrimitiveCounts[rvk_i][rvk_j]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBuildAccelerationStructuresKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("rvk_vkCmdBuildAccelerationStructuresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(infoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, pInfos[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(infoCount); ++rvk_i) {
        const uint32_t rvk_n = static_cast<uint32_t>(pInfos[rvk_i].geometryCount);
        for (uint32_t rvk_j = 0; rvk_j < rvk_n; ++rvk_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[rvk_i][rvk_j]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyAccelerationStructureKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCmdCopyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCmdCopyAccelerationStructureToMemoryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCmdCopyMemoryToAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawMeshTasksEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("rvk_vkCmdDrawMeshTasksEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawMeshTasksIndirectCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawMeshTasksIndirectCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawMeshTasksIndirectEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("rvk_vkCmdDrawMeshTasksIndirectEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdPushDescriptorSetKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    AV_SCOPE("rvk_vkCmdPushDescriptorSetKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, layout);
    payload.write_pod(set);
    payload.write_pod(descriptorWriteCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(descriptorWriteCount); ++rvk_i) {
        serialize_VkWriteDescriptorSet(payload, pDescriptorWrites[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetAlphaToCoverageEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    AV_SCOPE("rvk_vkCmdSetAlphaToCoverageEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToCoverageEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetAlphaToOneEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    AV_SCOPE("rvk_vkCmdSetAlphaToOneEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(alphaToOneEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetColorBlendAdvancedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    AV_SCOPE("rvk_vkCmdSetColorBlendAdvancedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkColorBlendAdvancedEXT(payload, pColorBlendAdvanced[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetColorBlendEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    AV_SCOPE("rvk_vkCmdSetColorBlendEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorBlendEnables[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetColorBlendEquationEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    AV_SCOPE("rvk_vkCmdSetColorBlendEquationEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        serialize_VkColorBlendEquationEXT(payload, pColorBlendEquations[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetColorWriteMaskEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    AV_SCOPE("rvk_vkCmdSetColorWriteMaskEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(attachmentCount); ++rvk_i) {
        payload.write_pod(pColorWriteMasks[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetConservativeRasterizationModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    AV_SCOPE("rvk_vkCmdSetConservativeRasterizationModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(conservativeRasterizationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageModulationModeNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    AV_SCOPE("rvk_vkCmdSetCoverageModulationModeNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageModulationTableEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    AV_SCOPE("rvk_vkCmdSetCoverageModulationTableEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageModulationTableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    AV_SCOPE("rvk_vkCmdSetCoverageModulationTableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageModulationTableCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(coverageModulationTableCount); ++rvk_i) {
        payload.write_pod(pCoverageModulationTable[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageReductionModeNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    AV_SCOPE("rvk_vkCmdSetCoverageReductionModeNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageReductionMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageToColorEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    AV_SCOPE("rvk_vkCmdSetCoverageToColorEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCoverageToColorLocationNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    AV_SCOPE("rvk_vkCmdSetCoverageToColorLocationNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(coverageToColorLocation);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetCullModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("rvk_vkCmdSetCullModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(cullMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCullModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthBoundsTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthBoundsTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthClampEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthClampEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthClampRangeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
    AV_SCOPE("rvk_vkCmdSetDepthClampRangeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClampMode);
    serialize_VkDepthClampRangeEXT(payload, *pDepthClampRange);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthClipEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthClipEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthClipEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthClipNegativeOneToOneEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    AV_SCOPE("rvk_vkCmdSetDepthClipNegativeOneToOneEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(negativeOneToOne);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthCompareOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("rvk_vkCmdSetDepthCompareOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthCompareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetDepthWriteEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("rvk_vkCmdSetDepthWriteEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(depthWriteEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetExtraPrimitiveOverestimationSizeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    AV_SCOPE("rvk_vkCmdSetExtraPrimitiveOverestimationSizeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(extraPrimitiveOverestimationSize);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetFragmentShadingRateKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) {
    AV_SCOPE("rvk_vkCmdSetFragmentShadingRateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkExtent2D(payload, *pFragmentSize);
    for (uint32_t rvk_i = 0; rvk_i < 2u; ++rvk_i) {
        payload.write_pod(combinerOps[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetFrontFaceEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("rvk_vkCmdSetFrontFaceEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(frontFace);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFaceEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLineRasterizationModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    AV_SCOPE("rvk_vkCmdSetLineRasterizationModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineRasterizationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLineStippleEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    AV_SCOPE("rvk_vkCmdSetLineStippleEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(lineStippleFactor);
    payload.write_pod(lineStipplePattern);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLineStippleEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    AV_SCOPE("rvk_vkCmdSetLineStippleEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stippledLineEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetLogicOpEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    AV_SCOPE("rvk_vkCmdSetLogicOpEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(logicOpEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPolygonModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    AV_SCOPE("rvk_vkCmdSetPolygonModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(polygonMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPolygonModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetPrimitiveTopologyEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("rvk_vkCmdSetPrimitiveTopologyEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(primitiveTopology);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetProvokingVertexModeEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    AV_SCOPE("rvk_vkCmdSetProvokingVertexModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(provokingVertexMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRasterizationSamplesEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    AV_SCOPE("rvk_vkCmdSetRasterizationSamplesEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationSamples);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRasterizationStreamEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    AV_SCOPE("rvk_vkCmdSetRasterizationStreamEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(rasterizationStream);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRayTracingPipelineStackSizeKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
    AV_SCOPE("rvk_vkCmdSetRayTracingPipelineStackSizeKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(pipelineStackSize);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRepresentativeFragmentTestEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    AV_SCOPE("rvk_vkCmdSetRepresentativeFragmentTestEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(representativeFragmentTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetSampleLocationsEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    AV_SCOPE("rvk_vkCmdSetSampleLocationsEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(sampleLocationsEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetSampleMaskEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    AV_SCOPE("rvk_vkCmdSetSampleMaskEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(samples);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>((samples + 31) / 32); ++rvk_i) {
        payload.write_pod(pSampleMask[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetSampleMaskEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetScissorWithCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("rvk_vkCmdSetScissorWithCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(scissorCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(scissorCount); ++rvk_i) {
        serialize_VkRect2D(payload, pScissors[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetShadingRateImageEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    AV_SCOPE("rvk_vkCmdSetShadingRateImageEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(shadingRateImageEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilOpEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("rvk_vkCmdSetStencilOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetStencilTestEnableEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("rvk_vkCmdSetStencilTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(stencilTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetTessellationDomainOriginEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    AV_SCOPE("rvk_vkCmdSetTessellationDomainOriginEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(domainOrigin);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetViewportSwizzleNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    AV_SCOPE("rvk_vkCmdSetViewportSwizzleNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewportSwizzleNV(payload, pViewportSwizzles[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetViewportWScalingEnableNV(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    AV_SCOPE("rvk_vkCmdSetViewportWScalingEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportWScalingEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetViewportWithCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("rvk_vkCmdSetViewportWithCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(viewportCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(viewportCount); ++rvk_i) {
        serialize_VkViewport(payload, pViewports[rvk_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdTraceRaysIndirectKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
    AV_SCOPE("rvk_vkCmdTraceRaysIndirectKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(indirectDeviceAddress);
    if (t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdTraceRaysKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
    AV_SCOPE("rvk_vkCmdTraceRaysKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(width);
    payload.write_pod(height);
    payload.write_pod(depth);
    if (t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
    AV_SCOPE("rvk_vkCmdWriteAccelerationStructuresPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(accelerationStructureCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(accelerationStructureCount); ++rvk_i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[rvk_i]);
    }
    payload.write_pod(queryType);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(firstQuery);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCopyAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCopyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCopyAccelerationStructureToMemoryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToImageEXT(::av::ClientTransport& t, VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
    AV_SCOPE("rvk_vkCopyImageToImageEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToImageInfo(payload, *pCopyImageToImageInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToImageEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyImageToMemoryEXT(::av::ClientTransport& t, VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
    AV_SCOPE("rvk_vkCopyImageToMemoryEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCopyImageToMemoryInfo(payload, *pCopyImageToMemoryInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyImageToMemoryEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkCopyMemoryToAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCopyMemoryToImageEXT(::av::ClientTransport& t, VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
    AV_SCOPE("rvk_vkCopyMemoryToImageEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkCopyMemoryToImageInfo(payload, *pCopyMemoryToImageInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCopyMemoryToImageEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) {
    AV_SCOPE("rvk_vkCreateAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateAccelerationStructureKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pAccelerationStructure)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateDeferredOperationKHR(::av::ClientTransport& t, VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) {
    AV_SCOPE("rvk_vkCreateDeferredOperationKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateDeferredOperationKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pDeferredOperation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateRayTracingPipelinesKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("rvk_vkCreateRayTracingPipelinesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, deferredOperation);
    serialize_VkPipelineCache(payload, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkRayTracingPipelineCreateInfoKHR(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pPipelines[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkCreateShadersEXT(::av::ClientTransport& t, VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) {
    AV_SCOPE("rvk_vkCreateShadersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(createInfoCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(createInfoCount); ++rvk_i) {
        serialize_VkShaderCreateInfoEXT(payload, pCreateInfos[rvk_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShadersEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(createInfoCount); ++rvk_k) {
        if (!r.read_pod(pShaders[rvk_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkDeferredOperationJoinKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("rvk_vkDeferredOperationJoinKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkDeferredOperationJoinKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyAccelerationStructureKHR(::av::ClientTransport& t, VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureKHR(payload, accelerationStructure);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyDeferredOperationKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyDeferredOperationKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDeferredOperationKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkDestroyShaderEXT(::av::ClientTransport& t, VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyShaderEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkShaderEXT(payload, shader);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyShaderEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkGetAccelerationStructureBuildSizesKHR(::av::ClientTransport& t, VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
    AV_SCOPE("rvk_vkGetAccelerationStructureBuildSizesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(buildType);
    serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, *pBuildInfo);
    const uint8_t pMaxPrimitiveCounts_present = pMaxPrimitiveCounts != nullptr ? 1 : 0;
    payload.write_pod(pMaxPrimitiveCounts_present);
    if (pMaxPrimitiveCounts_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(pBuildInfo->geometryCount); ++rvk_i) {
            payload.write_pod(pMaxPrimitiveCounts[rvk_i]);
        }
    }
    payload.write_pod(pSizeInfo->sType);
    serialize_pnext_chain(payload, pSizeInfo->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkAccelerationStructureBuildSizesInfoKHR(r, pSizeInfo, t.decode_arena())) {
        return;
    }
}

VkDeviceAddress rvk_vkGetAccelerationStructureDeviceAddressKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
    AV_SCOPE("rvk_vkGetAccelerationStructureDeviceAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureDeviceAddressInfoKHR(payload, *pInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureDeviceAddressKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<VkDeviceAddress>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<VkDeviceAddress>(0);
    }
    return static_cast<VkDeviceAddress>(result_code);
}

uint32_t rvk_vkGetDeferredOperationMaxConcurrencyKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("rvk_vkGetDeferredOperationMaxConcurrencyKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationMaxConcurrencyKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<uint32_t>(0);
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<uint32_t>(0);
    }
    return static_cast<uint32_t>(result_code);
}

VkResult rvk_vkGetDeferredOperationResultKHR(::av::ClientTransport& t, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("rvk_vkGetDeferredOperationResultKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeferredOperationKHR(payload, operation);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeferredOperationResultKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetDeviceAccelerationStructureCompatibilityKHR(::av::ClientTransport& t, VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
    AV_SCOPE("rvk_vkGetDeviceAccelerationStructureCompatibilityKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkAccelerationStructureVersionInfoKHR(payload, *pVersionInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceAccelerationStructureCompatibilityKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!r.read_pod(*pCompatibility)) {
        return;
    }
}

void rvk_vkGetImageSubresourceLayout2EXT(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("rvk_vkGetImageSubresourceLayout2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, pLayout, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPhysicalDeviceFragmentShadingRatesKHR(::av::ClientTransport& t, VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
    AV_SCOPE("rvk_vkGetPhysicalDeviceFragmentShadingRatesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pFragmentShadingRates != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pFragmentShadingRateCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFragmentShadingRatesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pFragmentShadingRateCount_answered = 0;
    if (!r.read_pod(pFragmentShadingRateCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pFragmentShadingRateCount_answered > *pFragmentShadingRateCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pFragmentShadingRateCount_answered; ++k) {
            if (!deserialize_VkPhysicalDeviceFragmentShadingRateKHR(r, &pFragmentShadingRates[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pFragmentShadingRateCount = pFragmentShadingRateCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("rvk_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<uint64_t>(dataSize);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetRayTracingShaderGroupHandlesKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("rvk_vkGetRayTracingShaderGroupHandlesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<uint64_t>(dataSize);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupHandlesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkDeviceSize rvk_vkGetRayTracingShaderGroupStackSizeKHR(::av::ClientTransport& t, VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
    AV_SCOPE("rvk_vkGetRayTracingShaderGroupStackSizeKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipeline(payload, pipeline);
    payload.write_pod(group);
    payload.write_pod(groupShader);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetRayTracingShaderGroupStackSizeKHR, payload, r) != ::av::TransportResult::ok) {
        return static_cast<VkDeviceSize>(0);
    }
    uint64_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return static_cast<VkDeviceSize>(0);
    }
    return static_cast<VkDeviceSize>(result_code);
}

VkResult rvk_vkTransitionImageLayoutEXT(::av::ClientTransport& t, VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
    AV_SCOPE("rvk_vkTransitionImageLayoutEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(transitionCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(transitionCount); ++rvk_i) {
        serialize_VkHostImageLayoutTransitionInfo(payload, pTransitions[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkTransitionImageLayoutEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
    AV_SCOPE("rvk_vkWriteAccelerationStructuresPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(accelerationStructureCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(accelerationStructureCount); ++rvk_i) {
        serialize_VkAccelerationStructureKHR(payload, pAccelerationStructures[rvk_i]);
    }
    payload.write_pod(queryType);
    payload.write_pod<uint64_t>(dataSize);
    payload.write_pod<uint64_t>(stride);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pData != nullptr && dataSize > 0) {
        if (!r.read(pData, static_cast<size_t>(dataSize))) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetCalibratedTimestampsEXT(::av::ClientTransport& t, VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    AV_SCOPE("rvk_vkGetCalibratedTimestampsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(timestampCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(timestampCount); ++rvk_i) {
        serialize_VkCalibratedTimestampInfoKHR(payload, pTimestampInfos[rvk_i]);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t rvk_k = 0; rvk_k < static_cast<uint32_t>(timestampCount); ++rvk_k) {
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
    AV_SCOPE("rvk_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, physicalDevice);
    const uint8_t array_present = pTimeDomains != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pTimeDomainCount);
    }
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint32_t pTimeDomainCount_answered = 0;
    if (!r.read_pod(pTimeDomainCount_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present) {
        if (pTimeDomainCount_answered > *pTimeDomainCount) {
            return VK_ERROR_DEVICE_LOST;
        }
        for (uint32_t k = 0; k < pTimeDomainCount_answered; ++k) {
            if (!r.read_pod(pTimeDomains[k])) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pTimeDomainCount = pTimeDomainCount_answered;
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdSetRenderingAttachmentLocationsKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
    AV_SCOPE("rvk_vkCmdSetRenderingAttachmentLocationsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingAttachmentLocationInfo(payload, *pLocationInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdSetRenderingInputAttachmentIndicesKHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
    AV_SCOPE("rvk_vkCmdSetRenderingInputAttachmentIndicesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkRenderingInputAttachmentIndexInfo(payload, *pInputAttachmentIndexInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindIndexBuffer2KHR(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
    AV_SCOPE("rvk_vkCmdBindIndexBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkBuffer(payload, buffer);
    payload.write_pod(offset);
    payload.write_pod(size);
    payload.write_pod(indexType);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkCreatePipelineBinariesKHR(::av::ClientTransport& t, VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
    AV_SCOPE("rvk_vkCreatePipelineBinariesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryCreateInfoKHR(payload, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    payload.write_pod(pBinaries->sType);
    serialize_pnext_chain(payload, pBinaries->pNext);
    payload.write_pod(pBinaries->pipelineBinaryCount);
    payload.write_pod<uint8_t>(pBinaries->pPipelineBinaries != nullptr ? 1 : 0);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineBinariesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkPipelineBinaryHandlesInfoKHR(r, pBinaries, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkDestroyPipelineBinaryKHR(::av::ClientTransport& t, VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkDestroyPipelineBinaryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineBinaryKHR(payload, pipelineBinary);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkGetDeviceImageSubresourceLayoutKHR(::av::ClientTransport& t, VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("rvk_vkGetDeviceImageSubresourceLayoutKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceImageSubresourceInfo(payload, *pInfo);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, pLayout, t.decode_arena())) {
        return;
    }
}

void rvk_vkGetImageSubresourceLayout2KHR(::av::ClientTransport& t, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("rvk_vkGetImageSubresourceLayout2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImage(payload, image);
    serialize_VkImageSubresource2(payload, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, pLayout->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, pLayout, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkGetPipelineKeyKHR(::av::ClientTransport& t, VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
    AV_SCOPE("rvk_vkGetPipelineKeyKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkPipelineCreateInfoKHR(payload, *pPipelineCreateInfo);
    payload.write_pod(pPipelineKey->sType);
    serialize_pnext_chain(payload, pPipelineKey->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineKeyKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkPipelineBinaryKeyKHR(r, pPipelineKey, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkGetRenderingAreaGranularityKHR(::av::ClientTransport& t, VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
    AV_SCOPE("rvk_vkGetRenderingAreaGranularityKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkRenderingAreaInfo(payload, *pRenderingAreaInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetRenderingAreaGranularityKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExtent2D(r, pGranularity, t.decode_arena())) {
        return;
    }
}

VkResult rvk_vkReleaseCapturedPipelineDataKHR(::av::ClientTransport& t, VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("rvk_vkReleaseCapturedPipelineDataKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkReleaseCapturedPipelineDataInfoKHR(payload, *pInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkReleaseCapturedPipelineDataKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void rvk_vkCmdBeginQueryIndexedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    AV_SCOPE("rvk_vkCmdBeginQueryIndexedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    payload.write_pod(index);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBeginTransformFeedbackEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("rvk_vkCmdBeginTransformFeedbackEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(counterBufferCount); ++rvk_i) {
        serialize_VkBuffer(payload, pCounterBuffers[rvk_i]);
    }
    const uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(counterBufferCount); ++rvk_i) {
            payload.write_pod(pCounterBufferOffsets[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdBindTransformFeedbackBuffersEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    AV_SCOPE("rvk_vkCmdBindTransformFeedbackBuffersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        serialize_VkBuffer(payload, pBuffers[rvk_i]);
    }
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
        payload.write_pod(pOffsets[rvk_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(bindingCount); ++rvk_i) {
            payload.write_pod(pSizes[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdDrawIndirectByteCountEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    AV_SCOPE("rvk_vkCmdDrawIndirectByteCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    serialize_VkBuffer(payload, counterBuffer);
    payload.write_pod(counterBufferOffset);
    payload.write_pod(counterOffset);
    payload.write_pod(vertexStride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndQueryIndexedEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    AV_SCOPE("rvk_vkCmdEndQueryIndexedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    serialize_VkQueryPool(payload, queryPool);
    payload.write_pod(query);
    payload.write_pod(index);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkCmdEndTransformFeedbackEXT(::av::ClientTransport& t, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("rvk_vkCmdEndTransformFeedbackEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(counterBufferCount); ++rvk_i) {
        serialize_VkBuffer(payload, pCounterBuffers[rvk_i]);
    }
    const uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (uint32_t rvk_i = 0; rvk_i < static_cast<uint32_t>(counterBufferCount); ++rvk_i) {
            payload.write_pod(pCounterBufferOffsets[rvk_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void rvk_vkSetDeviceMemoryPriorityEXT(::av::ClientTransport& t, VkDevice device, VkDeviceMemory memory, float priority) {
    AV_SCOPE("rvk_vkSetDeviceMemoryPriorityEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkDeviceMemory(payload, memory);
    payload.write_pod(priority);
    if (t.send_async(::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult rvk_vkGetFenceFdKHR(::av::ClientTransport& t, VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("rvk_vkGetFenceFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkFenceGetFdInfoKHR(payload, *pGetFdInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetFenceFdKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdKHR(::av::ClientTransport& t, VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("rvk_vkGetMemoryFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkMemoryGetFdInfoKHR(payload, *pGetFdInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetMemoryFdPropertiesKHR(::av::ClientTransport& t, VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    AV_SCOPE("rvk_vkGetMemoryFdPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    payload.write_pod(handleType);
    payload.write_pod(fd);
    payload.write_pod(pMemoryFdProperties->sType);
    serialize_pnext_chain(payload, pMemoryFdProperties->pNext);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkMemoryFdPropertiesKHR(r, pMemoryFdProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkGetSemaphoreFdKHR(::av::ClientTransport& t, VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("rvk_vkGetSemaphoreFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkSemaphoreGetFdInfoKHR(payload, *pGetFdInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetSemaphoreFdKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!r.read_pod(*pFd)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportFenceFdKHR(::av::ClientTransport& t, VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    AV_SCOPE("rvk_vkImportFenceFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImportFenceFdInfoKHR(payload, *pImportFenceFdInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkImportFenceFdKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkImportSemaphoreFdKHR(::av::ClientTransport& t, VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    AV_SCOPE("rvk_vkImportSemaphoreFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkImportSemaphoreFdInfoKHR(payload, *pImportSemaphoreFdInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkImportSemaphoreFdKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult rvk_vkUnmapMemory2KHR(::av::ClientTransport& t, VkDevice device, const VkMemoryUnmapInfo* pMemoryUnmapInfo) {
    AV_SCOPE("rvk_vkUnmapMemory2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, device);
    serialize_VkMemoryUnmapInfo(payload, *pMemoryUnmapInfo);
    const ::av::SyncCallScope rvk_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkUnmapMemory2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

}  // namespace av::vkproto
