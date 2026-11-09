// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_writer.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

void av_vkDestroyInstance(::av::ClientTransport& t, HandleTranslation& handles, VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyInstance");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, handles, instance);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyInstance, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyDevice(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDevice");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDevice, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkGetDeviceQueue(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, uint64_t* pQueue) {
    AV_SCOPE("av_vkGetDeviceQueue");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(queueFamilyIndex);
    payload.write_pod(queueIndex);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkEnumeratePhysicalDevices(::av::ClientTransport& t, HandleTranslation& handles, VkInstance instance, uint32_t* pPhysicalDeviceCount, uint64_t* pPhysicalDevices) {
    AV_SCOPE("av_vkEnumeratePhysicalDevices");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, handles, instance);
    const uint8_t array_present = pPhysicalDevices != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceCount);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, handles, pProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceProperties2(r, handles, pProperties, t.decode_arena())) {
        return;
    }
}

VkResult av_vkCreateInstance(::av::ClientTransport& t, HandleTranslation& handles, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, uint64_t* pInstance) {
    AV_SCOPE("av_vkCreateInstance");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstanceCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateDevice(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, uint64_t* pDevice) {
    AV_SCOPE("av_vkCreateDevice");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkDeviceCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceFeatures(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    AV_SCOPE("av_vkGetPhysicalDeviceFeatures");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceFormatProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(format);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetPhysicalDeviceImageFormatProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceImageFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(type);
    payload.write_pod(tiling);
    payload.write_pod(usage);
    payload.write_pod(flags);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceMemoryProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceMemoryProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceQueueFamilyProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceQueueFamilyProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceSparseImageFormatProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceSparseImageFormatProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
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
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkEnumerateDeviceExtensionProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    AV_SCOPE("av_vkEnumerateDeviceExtensionProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
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
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkEnumerateDeviceLayerProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    AV_SCOPE("av_vkEnumerateDeviceLayerProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceFeatures2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("av_vkGetPhysicalDeviceFeatures2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, handles, pFeatures->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceFeatures2(r, handles, pFeatures, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceFormatProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, handles, pFormatProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkFormatProperties2(r, handles, pFormatProperties, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetPhysicalDeviceImageFormatProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceImageFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, handles, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, handles, pImageFormatProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkImageFormatProperties2(r, handles, pImageFormatProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void av_vkGetPhysicalDeviceMemoryProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceMemoryProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, handles, pMemoryProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, handles, pMemoryProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceQueueFamilyProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceQueueFamilyProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkQueueFamilyProperties2(r, handles, &pQueueFamilyProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pQueueFamilyPropertyCount = pQueueFamilyPropertyCount_answered;
}

void av_vkGetPhysicalDeviceSparseImageFormatProperties2(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceSparseImageFormatProperties2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, handles, *pFormatInfo);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageFormatProperties2(r, handles, &pProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
}

VkResult av_vkDeviceWaitIdle(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device) {
    AV_SCOPE("av_vkDeviceWaitIdle");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkQueueWaitIdle(::av::ClientTransport& t, HandleTranslation& handles, VkQueue queue) {
    AV_SCOPE("av_vkQueueWaitIdle");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, handles, queue);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkAllocateMemory(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    AV_SCOPE("av_vkAllocateMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkMemoryAllocateInfo(payload, handles, *pAllocateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkFreeMemory(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkFreeMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceMemory(payload, handles, memory);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkFreeMemory, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkBindBufferMemory(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("av_vkBindBufferMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBuffer(payload, handles, buffer);
    serialize_VkDeviceMemory(payload, handles, memory);
    payload.write_pod(memoryOffset);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkBindImageMemory(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    AV_SCOPE("av_vkBindImageMemory");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    serialize_VkDeviceMemory(payload, handles, memory);
    payload.write_pod(memoryOffset);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetBufferMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("av_vkGetBufferMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBuffer(payload, handles, buffer);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetImageMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    AV_SCOPE("av_vkGetImageMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    AV_SCOPE("av_vkCreateBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBuffer(payload, handles, buffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateImage(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    AV_SCOPE("av_vkCreateImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyImage(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateImageView(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    AV_SCOPE("av_vkCreateImageView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageViewCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyImageView(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyImageView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageView(payload, handles, imageView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyImageView, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateSampler(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    AV_SCOPE("av_vkCreateSampler");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSamplerCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroySampler(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroySampler");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSampler(payload, handles, sampler);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySampler, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateSemaphore(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    AV_SCOPE("av_vkCreateSemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroySemaphore(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroySemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphore(payload, handles, semaphore);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySemaphore, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateFence(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    AV_SCOPE("av_vkCreateFence");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFenceCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyFence(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyFence");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFence(payload, handles, fence);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyFence, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkResetFences(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    AV_SCOPE("av_vkResetFences");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(fenceCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(fenceCount); ++av_i) {
        serialize_VkFence(payload, handles, pFences[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkResetFences, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult av_vkWaitForFences(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    AV_SCOPE("av_vkWaitForFences");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(fenceCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(fenceCount); ++av_i) {
        serialize_VkFence(payload, handles, pFences[av_i]);
    }
    payload.write_pod(waitAll);
    payload.write_pod(timeout);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetFenceStatus(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkFence fence) {
    AV_SCOPE("av_vkGetFenceStatus");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFence(payload, handles, fence);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateCommandPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    AV_SCOPE("av_vkCreateCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPoolCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyCommandPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPool(payload, handles, commandPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyCommandPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkResetCommandPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    AV_SCOPE("av_vkResetCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPool(payload, handles, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkResetCommandPool, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult av_vkBeginCommandBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    AV_SCOPE("av_vkBeginCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCommandBufferBeginInfo(payload, handles, *pBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkBeginCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult av_vkEndCommandBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer) {
    AV_SCOPE("av_vkEndCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkEndCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

VkResult av_vkResetCommandBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    AV_SCOPE("av_vkResetCommandBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkResetCommandBuffer, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void av_vkDestroyShaderModule(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyShaderModule");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderModule(payload, handles, shaderModule);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyShaderModule, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateShaderModule(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    AV_SCOPE("av_vkCreateShaderModule");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderModuleCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreatePipelineLayout(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    AV_SCOPE("av_vkCreatePipelineLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineLayoutCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyPipelineLayout(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyPipelineLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineLayout(payload, handles, pipelineLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineLayout, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyPipeline(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyPipeline");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipeline(payload, handles, pipeline);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipeline, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreatePipelineCache(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    AV_SCOPE("av_vkCreatePipelineCache");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCacheCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyPipelineCache(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyPipelineCache");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCache(payload, handles, pipelineCache);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineCache, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateRenderPass(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("av_vkCreateRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderPassCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyRenderPass(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderPass(payload, handles, renderPass);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateFramebuffer(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    AV_SCOPE("av_vkCreateFramebuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFramebufferCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyFramebuffer(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyFramebuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFramebuffer(payload, handles, framebuffer);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyFramebuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateDescriptorSetLayout(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    AV_SCOPE("av_vkCreateDescriptorSetLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyDescriptorSetLayout(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDescriptorSetLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSetLayout(payload, handles, descriptorSetLayout);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorSetLayout, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateDescriptorPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    AV_SCOPE("av_vkCreateDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorPoolCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyDescriptorPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorPool(payload, handles, descriptorPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkFreeCommandBuffers(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("av_vkFreeCommandBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPool(payload, handles, commandPool);
    payload.write_pod(commandBufferCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(commandBufferCount); ++av_i) {
        serialize_VkCommandBuffer(payload, handles, pCommandBuffers[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkFreeCommandBuffers, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkQueueSubmit(::av::ClientTransport& t, HandleTranslation& handles, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    AV_SCOPE("av_vkQueueSubmit");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, handles, queue);
    payload.write_pod(submitCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(submitCount); ++av_i) {
        serialize_VkSubmitInfo(payload, handles, pSubmits[av_i]);
    }
    serialize_VkFence(payload, handles, fence);
    if (t.send_async(::av::vkproto::kCmd_vkQueueSubmit, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void av_vkCmdBindPipeline(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    AV_SCOPE("av_vkCmdBindPipeline");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipeline(payload, handles, pipeline);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindPipeline, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetViewport(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("av_vkCmdSetViewport");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(viewportCount); ++av_i) {
        serialize_VkViewport(payload, pViewports[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewport, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetScissor(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("av_vkCmdSetScissor");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstScissor);
    payload.write_pod(scissorCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(scissorCount); ++av_i) {
        serialize_VkRect2D(payload, pScissors[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissor, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindVertexBuffers(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    AV_SCOPE("av_vkCmdBindVertexBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pBuffers[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        payload.write_pod(pOffsets[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindIndexBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    AV_SCOPE("av_vkCmdBindIndexBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    payload.write_pod(indexType);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindDescriptorSets(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    AV_SCOPE("av_vkCmdBindDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, handles, layout);
    payload.write_pod(firstSet);
    payload.write_pod(descriptorSetCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(descriptorSetCount); ++av_i) {
        serialize_VkDescriptorSet(payload, handles, pDescriptorSets[av_i]);
    }
    payload.write_pod(dynamicOffsetCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(dynamicOffsetCount); ++av_i) {
        payload.write_pod(pDynamicOffsets[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorSets, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDraw(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    AV_SCOPE("av_vkCmdDraw");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(vertexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstVertex);
    payload.write_pod(firstInstance);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDraw, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndexed(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    AV_SCOPE("av_vkCmdDrawIndexed");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(indexCount);
    payload.write_pod(instanceCount);
    payload.write_pod(firstIndex);
    payload.write_pod(vertexOffset);
    payload.write_pod(firstInstance);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexed, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPipelineBarrier(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("av_vkCmdPipelineBarrier");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(dependencyFlags);
    payload.write_pod(memoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(memoryBarrierCount); ++av_i) {
        serialize_VkMemoryBarrier(payload, handles, pMemoryBarriers[av_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bufferMemoryBarrierCount); ++av_i) {
        serialize_VkBufferMemoryBarrier(payload, handles, pBufferMemoryBarriers[av_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(imageMemoryBarrierCount); ++av_i) {
        serialize_VkImageMemoryBarrier(payload, handles, pImageMemoryBarriers[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    AV_SCOPE("av_vkCmdCopyBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, srcBuffer);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkBufferCopy(payload, pRegions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBeginRenderPass(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    AV_SCOPE("av_vkCmdBeginRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, handles, *pRenderPassBegin);
    payload.write_pod(contents);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndRenderPass(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer) {
    AV_SCOPE("av_vkCmdEndRenderPass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdNextSubpass(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    AV_SCOPE("av_vkCmdNextSubpass");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(contents);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushConstants(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    AV_SCOPE("av_vkCmdPushConstants");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkPipelineLayout(payload, handles, layout);
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

VkResult av_vkCreateGraphicsPipelines(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("av_vkCreateGraphicsPipelines");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCache(payload, handles, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(createInfoCount); ++av_i) {
        serialize_VkGraphicsPipelineCreateInfo(payload, handles, pCreateInfos[av_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateGraphicsPipelines, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(createInfoCount); ++av_k) {
        if (!r.read_pod(pPipelines[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

void av_vkUpdateDescriptorSets(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    AV_SCOPE("av_vkUpdateDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(descriptorWriteCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(descriptorWriteCount); ++av_i) {
        serialize_VkWriteDescriptorSet(payload, handles, pDescriptorWrites[av_i]);
    }
    payload.write_pod(descriptorCopyCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(descriptorCopyCount); ++av_i) {
        serialize_VkCopyDescriptorSet(payload, handles, pDescriptorCopies[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSets, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkAllocateCommandBuffers(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, uint64_t* pCommandBuffers) {
    AV_SCOPE("av_vkAllocateCommandBuffers");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandBufferAllocateInfo(payload, handles, *pAllocateInfo);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateCommandBuffers, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(pAllocateInfo->commandBufferCount); ++av_k) {
        if (!r.read_pod(pCommandBuffers[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkAllocateDescriptorSets(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("av_vkAllocateDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSetAllocateInfo(payload, handles, *pAllocateInfo);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkAllocateDescriptorSets, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(pAllocateInfo->descriptorSetCount); ++av_k) {
        if (!r.read_pod(pDescriptorSets[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkFreeDescriptorSets(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    AV_SCOPE("av_vkFreeDescriptorSets");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorPool(payload, handles, descriptorPool);
    payload.write_pod(descriptorSetCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(descriptorSetCount); ++av_i) {
        serialize_VkDescriptorSet(payload, handles, pDescriptorSets[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateComputePipelines(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("av_vkCreateComputePipelines");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCache(payload, handles, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(createInfoCount); ++av_i) {
        serialize_VkComputePipelineCreateInfo(payload, handles, pCreateInfos[av_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateComputePipelines, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(createInfoCount); ++av_k) {
        if (!r.read_pod(pPipelines[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkResetDescriptorPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    AV_SCOPE("av_vkResetDescriptorPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorPool(payload, handles, descriptorPool);
    payload.write_pod(flags);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateEvent(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    AV_SCOPE("av_vkCreateEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkEventCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyEvent(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkEvent(payload, handles, event);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkGetEventStatus(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkEvent event) {
    AV_SCOPE("av_vkGetEventStatus");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkEvent(payload, handles, event);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkSetEvent(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkEvent event) {
    AV_SCOPE("av_vkSetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkEvent(payload, handles, event);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkResetEvent(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkEvent event) {
    AV_SCOPE("av_vkResetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkEvent(payload, handles, event);
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdSetEvent(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("av_vkCmdSetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResetEvent(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    AV_SCOPE("av_vkCmdResetEvent");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWaitEvents(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    AV_SCOPE("av_vkCmdWaitEvents");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(eventCount); ++av_i) {
        serialize_VkEvent(payload, handles, pEvents[av_i]);
    }
    payload.write_pod(srcStageMask);
    payload.write_pod(dstStageMask);
    payload.write_pod(memoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(memoryBarrierCount); ++av_i) {
        serialize_VkMemoryBarrier(payload, handles, pMemoryBarriers[av_i]);
    }
    payload.write_pod(bufferMemoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bufferMemoryBarrierCount); ++av_i) {
        serialize_VkBufferMemoryBarrier(payload, handles, pBufferMemoryBarriers[av_i]);
    }
    payload.write_pod(imageMemoryBarrierCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(imageMemoryBarrierCount); ++av_i) {
        serialize_VkImageMemoryBarrier(payload, handles, pImageMemoryBarriers[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    AV_SCOPE("av_vkCmdCopyImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, handles, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkImageCopy(payload, pRegions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBufferToImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("av_vkCmdCopyBufferToImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, srcBuffer);
    serialize_VkImage(payload, handles, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkBufferImageCopy(payload, pRegions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImageToBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    AV_SCOPE("av_vkCmdCopyImageToBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkBufferImageCopy(payload, pRegions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBlitImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    AV_SCOPE("av_vkCmdBlitImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, handles, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkImageBlit(payload, pRegions[av_i]);
    }
    payload.write_pod(filter);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdClearColorImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("av_vkCmdClearColorImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, image);
    payload.write_pod(imageLayout);
    serialize_VkClearColorValue(payload, *pColor);
    payload.write_pod(rangeCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(rangeCount); ++av_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearColorImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdClearDepthStencilImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    AV_SCOPE("av_vkCmdClearDepthStencilImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, image);
    payload.write_pod(imageLayout);
    serialize_VkClearDepthStencilValue(payload, *pDepthStencil);
    payload.write_pod(rangeCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(rangeCount); ++av_i) {
        serialize_VkImageSubresourceRange(payload, pRanges[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearDepthStencilImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdClearAttachments(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    AV_SCOPE("av_vkCmdClearAttachments");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        serialize_VkClearAttachment(payload, pAttachments[av_i]);
    }
    payload.write_pod(rectCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(rectCount); ++av_i) {
        serialize_VkClearRect(payload, pRects[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdClearAttachments, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdFillBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    AV_SCOPE("av_vkCmdFillBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(size);
    payload.write_pod(data);
    if (t.send_async(::av::vkproto::kCmd_vkCmdFillBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdUpdateBuffer(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    AV_SCOPE("av_vkCmdUpdateBuffer");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(dataSize);
    if (pData != nullptr && dataSize > 0) {
        payload.write(pData, static_cast<size_t>(dataSize));
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdUpdateBuffer, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLineWidth(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, float lineWidth) {
    AV_SCOPE("av_vkCmdSetLineWidth");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(lineWidth);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineWidth, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBias(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    AV_SCOPE("av_vkCmdSetDepthBias");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthBiasConstantFactor);
    payload.write_pod(depthBiasClamp);
    payload.write_pod(depthBiasSlopeFactor);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBias, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetBlendConstants(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const float* blendConstants) {
    AV_SCOPE("av_vkCmdSetBlendConstants");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    for (uint32_t av_i = 0; av_i < 4u; ++av_i) {
        payload.write_pod(blendConstants[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetBlendConstants, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBounds(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    AV_SCOPE("av_vkCmdSetDepthBounds");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(minDepthBounds);
    payload.write_pod(maxDepthBounds);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBounds, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilCompareMask(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    AV_SCOPE("av_vkCmdSetStencilCompareMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(compareMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilCompareMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilWriteMask(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    AV_SCOPE("av_vkCmdSetStencilWriteMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(writeMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilWriteMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilReference(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    AV_SCOPE("av_vkCmdSetStencilReference");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(reference);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilReference, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDispatch(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("av_vkCmdDispatch");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatch, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDispatchIndirect(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    AV_SCOPE("av_vkCmdDispatchIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDispatchIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndirect(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndexedIndirect(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndexedIndirect");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirect, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdExecuteCommands(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    AV_SCOPE("av_vkCmdExecuteCommands");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(commandBufferCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(commandBufferCount); ++av_i) {
        serialize_VkCommandBuffer(payload, handles, pCommandBuffers[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdExecuteCommands, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkAcquireNextImage2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    AV_SCOPE("av_vkAcquireNextImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAcquireNextImageInfoKHR(payload, handles, *pAcquireInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkBindBufferMemory2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("av_vkBindBufferMemory2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindInfoCount); ++av_i) {
        serialize_VkBindBufferMemoryInfo(payload, handles, pBindInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkBindImageMemory2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("av_vkBindImageMemory2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindInfoCount); ++av_i) {
        serialize_VkBindImageMemoryInfo(payload, handles, pBindInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdBeginQuery(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    AV_SCOPE("av_vkCmdBeginQuery");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginQuery, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBeginRenderPass2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("av_vkCmdBeginRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, handles, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, handles, *pSubpassBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBeginRendering(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("av_vkCmdBeginRendering");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderingInfo(payload, handles, *pRenderingInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRendering, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindVertexBuffers2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("av_vkCmdBindVertexBuffers2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pBuffers[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        payload.write_pod(pOffsets[av_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
            payload.write_pod(pSizes[av_i]);
        }
    }
    const uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
            payload.write_pod(pStrides[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBlitImage2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("av_vkCmdBlitImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBlitImageInfo2(payload, handles, *pBlitImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBuffer2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("av_vkCmdCopyBuffer2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, handles, *pCopyBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBufferToImage2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("av_vkCmdCopyBufferToImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, handles, *pCopyBufferToImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImage2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("av_vkCmdCopyImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyImageInfo2(payload, handles, *pCopyImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImageToBuffer2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("av_vkCmdCopyImageToBuffer2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, handles, *pCopyImageToBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyQueryPoolResults(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("av_vkCmdCopyQueryPoolResults");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(stride);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyQueryPoolResults, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDispatchBase(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("av_vkCmdDispatchBase");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
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

void av_vkCmdDrawIndexedIndirectCount(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndexedIndirectCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, handles, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndirectCount(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndirectCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, handles, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndQuery(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("av_vkCmdEndQuery");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndQuery, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndRenderPass2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("av_vkCmdEndRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSubpassEndInfo(payload, handles, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndRendering(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer) {
    AV_SCOPE("av_vkCmdEndRendering");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRendering, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdNextSubpass2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("av_vkCmdNextSubpass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, handles, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, handles, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPipelineBarrier2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("av_vkCmdPipelineBarrier2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkDependencyInfo(payload, handles, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResetEvent2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("av_vkCmdResetEvent2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResetQueryPool(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("av_vkCmdResetQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResolveImage(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    AV_SCOPE("av_vkCmdResolveImage");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkImage(payload, handles, srcImage);
    payload.write_pod(srcImageLayout);
    serialize_VkImage(payload, handles, dstImage);
    payload.write_pod(dstImageLayout);
    payload.write_pod(regionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(regionCount); ++av_i) {
        serialize_VkImageResolve(payload, pRegions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResolveImage2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("av_vkCmdResolveImage2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkResolveImageInfo2(payload, handles, *pResolveImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCullMode(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("av_vkCmdSetCullMode");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(cullMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCullMode, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBiasEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("av_vkCmdSetDepthBiasEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthBiasEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBoundsTestEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("av_vkCmdSetDepthBoundsTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthCompareOp(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("av_vkCmdSetDepthCompareOp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthCompareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthTestEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("av_vkCmdSetDepthTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthWriteEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("av_vkCmdSetDepthWriteEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthWriteEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDeviceMask(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("av_vkCmdSetDeviceMask");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(deviceMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMask, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetEvent2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("av_vkCmdSetEvent2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    serialize_VkDependencyInfo(payload, handles, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetFrontFace(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("av_vkCmdSetFrontFace");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(frontFace);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFace, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPrimitiveRestartEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("av_vkCmdSetPrimitiveRestartEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPrimitiveTopology(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("av_vkCmdSetPrimitiveTopology");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(primitiveTopology);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopology, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRasterizerDiscardEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("av_vkCmdSetRasterizerDiscardEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetScissorWithCount(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("av_vkCmdSetScissorWithCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(scissorCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(scissorCount); ++av_i) {
        serialize_VkRect2D(payload, pScissors[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilOp(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("av_vkCmdSetStencilOp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilTestEnable(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("av_vkCmdSetStencilTestEnable");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stencilTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnable, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetViewportWithCount(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("av_vkCmdSetViewportWithCount");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(viewportCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(viewportCount); ++av_i) {
        serialize_VkViewport(payload, pViewports[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCount, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWaitEvents2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("av_vkCmdWaitEvents2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(eventCount); ++av_i) {
        serialize_VkEvent(payload, handles, pEvents[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(eventCount); ++av_i) {
        serialize_VkDependencyInfo(payload, handles, pDependencyInfos[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWriteTimestamp(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("av_vkCmdWriteTimestamp");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineStage);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWriteTimestamp2(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("av_vkCmdWriteTimestamp2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateBufferView(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    AV_SCOPE("av_vkCreateBufferView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferViewCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateDescriptorUpdateTemplate(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("av_vkCreateDescriptorUpdateTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreatePrivateDataSlot(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    AV_SCOPE("av_vkCreatePrivateDataSlot");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPrivateDataSlotCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateQueryPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    AV_SCOPE("av_vkCreateQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkQueryPoolCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateRenderPass2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("av_vkCreateRenderPass2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderPassCreateInfo2(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateSamplerYcbcrConversion(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("av_vkCreateSamplerYcbcrConversion");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyBufferView(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyBufferView");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferView(payload, handles, bufferView);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyBufferView, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyDescriptorUpdateTemplate(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDescriptorUpdateTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorUpdateTemplate(payload, handles, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyPrivateDataSlot(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyPrivateDataSlot");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPrivateDataSlot(payload, handles, privateDataSlot);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPrivateDataSlot, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyQueryPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkQueryPool(payload, handles, queryPool);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroySamplerYcbcrConversion(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroySamplerYcbcrConversion");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSamplerYcbcrConversion(payload, handles, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkEnumeratePhysicalDeviceGroups(::av::ClientTransport& t, HandleTranslation& handles, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("av_vkEnumeratePhysicalDeviceGroups");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, handles, instance);
    const uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceGroupCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, handles, &pPhysicalDeviceGroupProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult av_vkFlushMappedMemoryRanges(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("av_vkFlushMappedMemoryRanges");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(memoryRangeCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(memoryRangeCount); ++av_i) {
        serialize_VkMappedMemoryRange(payload, handles, pMemoryRanges[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkDeviceAddress av_vkGetBufferDeviceAddress(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetBufferDeviceAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferDeviceAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetBufferMemoryRequirements2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetBufferMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

uint64_t av_vkGetBufferOpaqueCaptureAddress(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetBufferOpaqueCaptureAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferDeviceAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDescriptorSetLayoutSupport(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("av_vkGetDescriptorSetLayoutSupport");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, handles, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, handles, pSupport->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkDescriptorSetLayoutSupport(r, handles, pSupport, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceBufferMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceBufferMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceGroupPeerMemoryFeatures(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("av_vkGetDeviceGroupPeerMemoryFeatures");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetDeviceGroupPresentCapabilitiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    AV_SCOPE("av_vkGetDeviceGroupPresentCapabilitiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(pDeviceGroupPresentCapabilities->sType);
    serialize_pnext_chain(payload, handles, pDeviceGroupPresentCapabilities->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkDeviceGroupPresentCapabilitiesKHR(r, handles, pDeviceGroupPresentCapabilities, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetDeviceGroupSurfacePresentModesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    AV_SCOPE("av_vkGetDeviceGroupSurfacePresentModesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSurfaceKHR(payload, handles, surface);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDeviceImageMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceImageMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceImageMemoryRequirements(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceImageSparseMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceImageSparseMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceImageMemoryRequirements(payload, handles, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageMemoryRequirements2(r, handles, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void av_vkGetDeviceMemoryCommitment(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    AV_SCOPE("av_vkGetDeviceMemoryCommitment");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceMemory(payload, handles, memory);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_vkGetDeviceMemoryOpaqueCaptureAddress(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetDeviceMemoryOpaqueCaptureAddress");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDeviceQueue2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, uint64_t* pQueue) {
    AV_SCOPE("av_vkGetDeviceQueue2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceQueueInfo2(payload, handles, *pQueueInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetImageMemoryRequirements2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetImageMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetImageSparseMemoryRequirements(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    AV_SCOPE("av_vkGetImageSparseMemoryRequirements");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetImageSparseMemoryRequirements2(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("av_vkGetImageSparseMemoryRequirements2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, handles, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageMemoryRequirements2(r, handles, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void av_vkGetImageSubresourceLayout(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    AV_SCOPE("av_vkGetImageSubresourceLayout");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    serialize_VkImageSubresource(payload, *pSubresource);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceExternalBufferProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalBufferProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, handles, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalBufferProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalBufferProperties(r, handles, pExternalBufferProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceExternalFenceProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalFenceProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, handles, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalFenceProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalFenceProperties(r, handles, pExternalFenceProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceExternalSemaphoreProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalSemaphoreProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, handles, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalSemaphoreProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalSemaphoreProperties(r, handles, pExternalSemaphoreProperties, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetPhysicalDevicePresentRectanglesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    AV_SCOPE("av_vkGetPhysicalDevicePresentRectanglesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkSurfaceKHR(payload, handles, surface);
    const uint8_t array_present = pRects != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pRectCount);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetPhysicalDeviceToolProperties(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceToolProperties");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pToolProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pToolCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkPhysicalDeviceToolProperties(r, handles, &pToolProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pToolCount = pToolCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetPipelineCacheData(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    AV_SCOPE("av_vkGetPipelineCacheData");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCache(payload, handles, pipelineCache);
    const uint8_t array_present = pData != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint64_t>(*pDataSize);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineCacheData, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint64_t pDataSize_answered = 0;
    if (!r.read_pod(pDataSize_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present && (pDataSize_answered > *pDataSize ||
        (pDataSize_answered != 0 && !r.read(pData, static_cast<size_t>(pDataSize_answered))))) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pDataSize_answered > static_cast<uint64_t>(~size_t{0})) {
        return VK_ERROR_DEVICE_LOST;
    }
    *pDataSize = static_cast<size_t>(pDataSize_answered);
    return static_cast<VkResult>(result_code);
}

void av_vkGetPrivateData(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    AV_SCOPE("av_vkGetPrivateData");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, handles, privateDataSlot);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetQueryPoolResults(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    AV_SCOPE("av_vkGetQueryPoolResults");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    payload.write_pod<uint64_t>(dataSize);
    payload.write_pod(stride);
    payload.write_pod(flags);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetRenderAreaGranularity(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    AV_SCOPE("av_vkGetRenderAreaGranularity");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderPass(payload, handles, renderPass);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetSemaphoreCounterValue(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("av_vkGetSemaphoreCounterValue");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphore(payload, handles, semaphore);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkInvalidateMappedMemoryRanges(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    AV_SCOPE("av_vkInvalidateMappedMemoryRanges");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(memoryRangeCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(memoryRangeCount); ++av_i) {
        serialize_VkMappedMemoryRange(payload, handles, pMemoryRanges[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkMergePipelineCaches(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    AV_SCOPE("av_vkMergePipelineCaches");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCache(payload, handles, dstCache);
    payload.write_pod(srcCacheCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(srcCacheCount); ++av_i) {
        serialize_VkPipelineCache(payload, handles, pSrcCaches[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkQueueBindSparse(::av::ClientTransport& t, HandleTranslation& handles, VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    AV_SCOPE("av_vkQueueBindSparse");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, handles, queue);
    payload.write_pod(bindInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindInfoCount); ++av_i) {
        serialize_VkBindSparseInfo(payload, handles, pBindInfo[av_i]);
    }
    serialize_VkFence(payload, handles, fence);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkQueueSubmit2(::av::ClientTransport& t, HandleTranslation& handles, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("av_vkQueueSubmit2");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, handles, queue);
    payload.write_pod(submitCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(submitCount); ++av_i) {
        serialize_VkSubmitInfo2(payload, handles, pSubmits[av_i]);
    }
    serialize_VkFence(payload, handles, fence);
    if (t.send_async(::av::vkproto::kCmd_vkQueueSubmit2, payload) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    return VK_SUCCESS;
}

void av_vkResetQueryPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    AV_SCOPE("av_vkResetQueryPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(firstQuery);
    payload.write_pod(queryCount);
    if (t.send_async(::av::vkproto::kCmd_vkResetQueryPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkSetPrivateData(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    AV_SCOPE("av_vkSetPrivateData");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(objectType);
    payload.write_pod(objectHandle);
    serialize_VkPrivateDataSlot(payload, handles, privateDataSlot);
    payload.write_pod(data);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkSignalSemaphore(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("av_vkSignalSemaphore");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreSignalInfo(payload, handles, *pSignalInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkTrimCommandPool(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("av_vkTrimCommandPool");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPool(payload, handles, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkTrimCommandPool, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkUpdateDescriptorSetWithTemplate(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("av_vkUpdateDescriptorSetWithTemplate");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSet(payload, handles, descriptorSet);
    serialize_VkDescriptorUpdateTemplate(payload, handles, descriptorUpdateTemplate);
    const uint64_t av_blob = (pData != nullptr) ? handles.template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(av_blob);
    if (av_blob != 0) { payload.write(pData, static_cast<size_t>(av_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplate, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkWaitSemaphores(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("av_vkWaitSemaphores");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreWaitInfo(payload, handles, *pWaitInfo);
    payload.write_pod(timeout);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceFeatures2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    AV_SCOPE("av_vkGetPhysicalDeviceFeatures2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pFeatures->sType);
    serialize_pnext_chain(payload, handles, pFeatures->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceFeatures2(r, handles, pFeatures, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceFormatProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(format);
    payload.write_pod(pFormatProperties->sType);
    serialize_pnext_chain(payload, handles, pFormatProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkFormatProperties2(r, handles, pFormatProperties, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetPhysicalDeviceImageFormatProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceImageFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceImageFormatInfo2(payload, handles, *pImageFormatInfo);
    payload.write_pod(pImageFormatProperties->sType);
    serialize_pnext_chain(payload, handles, pImageFormatProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkImageFormatProperties2(r, handles, pImageFormatProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void av_vkGetPhysicalDeviceMemoryProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceMemoryProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pMemoryProperties->sType);
    serialize_pnext_chain(payload, handles, pMemoryProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceMemoryProperties2(r, handles, pMemoryProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(pProperties->sType);
    serialize_pnext_chain(payload, handles, pProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkPhysicalDeviceProperties2(r, handles, pProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceQueueFamilyProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pQueueFamilyProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pQueueFamilyPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkQueueFamilyProperties2(r, handles, &pQueueFamilyProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pQueueFamilyPropertyCount = pQueueFamilyPropertyCount_answered;
}

void av_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceSparseImageFormatInfo2(payload, handles, *pFormatInfo);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageFormatProperties2(r, handles, &pProperties[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
}

VkResult av_vkBindBufferMemory2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    AV_SCOPE("av_vkBindBufferMemory2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindInfoCount); ++av_i) {
        serialize_VkBindBufferMemoryInfo(payload, handles, pBindInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkBindImageMemory2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    AV_SCOPE("av_vkBindImageMemory2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(bindInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindInfoCount); ++av_i) {
        serialize_VkBindImageMemoryInfo(payload, handles, pBindInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetBufferMemoryRequirements2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetBufferMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferMemoryRequirementsInfo2(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetImageMemoryRequirements2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetImageMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageMemoryRequirementsInfo2(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetImageSparseMemoryRequirements2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("av_vkGetImageSparseMemoryRequirements2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImageSparseMemoryRequirementsInfo2(payload, handles, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageMemoryRequirements2(r, handles, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

void av_vkCmdBeginRenderPass2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    AV_SCOPE("av_vkCmdBeginRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderPassBeginInfo(payload, handles, *pRenderPassBegin);
    serialize_VkSubpassBeginInfo(payload, handles, *pSubpassBeginInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBeginRenderingKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    AV_SCOPE("av_vkCmdBeginRenderingKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderingInfo(payload, handles, *pRenderingInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginRenderingKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBlitImage2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    AV_SCOPE("av_vkCmdBlitImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBlitImageInfo2(payload, handles, *pBlitImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBlitImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBuffer2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    AV_SCOPE("av_vkCmdCopyBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyBufferInfo2(payload, handles, *pCopyBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyBufferToImage2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    AV_SCOPE("av_vkCmdCopyBufferToImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyBufferToImageInfo2(payload, handles, *pCopyBufferToImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImage2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    AV_SCOPE("av_vkCmdCopyImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyImageInfo2(payload, handles, *pCopyImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyImageToBuffer2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    AV_SCOPE("av_vkCmdCopyImageToBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyImageToBufferInfo2(payload, handles, *pCopyImageToBufferInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDispatchBaseKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("av_vkCmdDispatchBaseKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
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

void av_vkCmdDrawIndexedIndirectCountKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndexedIndirectCountKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, handles, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndirectCountKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawIndirectCountKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, handles, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndRenderPass2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("av_vkCmdEndRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSubpassEndInfo(payload, handles, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderPass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndRenderingKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer) {
    AV_SCOPE("av_vkCmdEndRenderingKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndRenderingKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdNextSubpass2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    AV_SCOPE("av_vkCmdNextSubpass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSubpassBeginInfo(payload, handles, *pSubpassBeginInfo);
    serialize_VkSubpassEndInfo(payload, handles, *pSubpassEndInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdNextSubpass2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPipelineBarrier2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("av_vkCmdPipelineBarrier2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkDependencyInfo(payload, handles, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushDescriptorSetWithTemplateKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
    AV_SCOPE("av_vkCmdPushDescriptorSetWithTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkDescriptorUpdateTemplate(payload, handles, descriptorUpdateTemplate);
    serialize_VkPipelineLayout(payload, handles, layout);
    payload.write_pod(set);
    const uint64_t av_blob = (pData != nullptr) ? handles.template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(av_blob);
    if (av_blob != 0) { payload.write(pData, static_cast<size_t>(av_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetWithTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResetEvent2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    AV_SCOPE("av_vkCmdResetEvent2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    payload.write_pod(stageMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResetEvent2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdResolveImage2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    AV_SCOPE("av_vkCmdResolveImage2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkResolveImageInfo2(payload, handles, *pResolveImageInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdResolveImage2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDeviceMaskKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    AV_SCOPE("av_vkCmdSetDeviceMaskKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(deviceMask);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetEvent2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    AV_SCOPE("av_vkCmdSetEvent2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkEvent(payload, handles, event);
    serialize_VkDependencyInfo(payload, handles, *pDependencyInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetEvent2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWaitEvents2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    AV_SCOPE("av_vkCmdWaitEvents2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(eventCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(eventCount); ++av_i) {
        serialize_VkEvent(payload, handles, pEvents[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(eventCount); ++av_i) {
        serialize_VkDependencyInfo(payload, handles, pDependencyInfos[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdWaitEvents2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWriteTimestamp2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    AV_SCOPE("av_vkCmdWriteTimestamp2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stage);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreateDescriptorUpdateTemplateKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    AV_SCOPE("av_vkCreateDescriptorUpdateTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorUpdateTemplateCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateRenderPass2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    AV_SCOPE("av_vkCreateRenderPass2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderPassCreateInfo2(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateSamplerYcbcrConversionKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    AV_SCOPE("av_vkCreateSamplerYcbcrConversionKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSamplerYcbcrConversionCreateInfo(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyDescriptorUpdateTemplateKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDescriptorUpdateTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorUpdateTemplate(payload, handles, descriptorUpdateTemplate);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroySamplerYcbcrConversionKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroySamplerYcbcrConversionKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSamplerYcbcrConversion(payload, handles, ycbcrConversion);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkEnumeratePhysicalDeviceGroupsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    AV_SCOPE("av_vkEnumeratePhysicalDeviceGroupsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkInstance(payload, handles, instance);
    const uint8_t array_present = pPhysicalDeviceGroupProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPhysicalDeviceGroupCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkPhysicalDeviceGroupProperties(r, handles, &pPhysicalDeviceGroupProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount_answered;
    return static_cast<VkResult>(result_code);
}

VkDeviceAddress av_vkGetBufferDeviceAddressKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetBufferDeviceAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferDeviceAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

uint64_t av_vkGetBufferOpaqueCaptureAddressKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetBufferOpaqueCaptureAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkBufferDeviceAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDescriptorSetLayoutSupportKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    AV_SCOPE("av_vkGetDescriptorSetLayoutSupportKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSetLayoutCreateInfo(payload, handles, *pCreateInfo);
    payload.write_pod(pSupport->sType);
    serialize_pnext_chain(payload, handles, pSupport->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkDescriptorSetLayoutSupport(r, handles, pSupport, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceBufferMemoryRequirementsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceBufferMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceBufferMemoryRequirements(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceGroupPeerMemoryFeaturesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    AV_SCOPE("av_vkGetDeviceGroupPeerMemoryFeaturesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(heapIndex);
    payload.write_pod(localDeviceIndex);
    payload.write_pod(remoteDeviceIndex);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDeviceImageMemoryRequirementsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceImageMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceImageMemoryRequirements(payload, handles, *pInfo);
    payload.write_pod(pMemoryRequirements->sType);
    serialize_pnext_chain(payload, handles, pMemoryRequirements->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMemoryRequirements2(r, handles, pMemoryRequirements, t.decode_arena())) {
        return;
    }
}

void av_vkGetDeviceImageSparseMemoryRequirementsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    AV_SCOPE("av_vkGetDeviceImageSparseMemoryRequirementsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceImageMemoryRequirements(payload, handles, *pInfo);
    const uint8_t array_present = pSparseMemoryRequirements != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pSparseMemoryRequirementCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkSparseImageMemoryRequirements2(r, handles, &pSparseMemoryRequirements[k], t.decode_arena())) {
            return;
            }
        }
    }
    *pSparseMemoryRequirementCount = pSparseMemoryRequirementCount_answered;
}

uint64_t av_vkGetDeviceMemoryOpaqueCaptureAddressKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    AV_SCOPE("av_vkGetDeviceMemoryOpaqueCaptureAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceMemoryOpaqueCaptureAddressInfo(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetPhysicalDeviceExternalBufferPropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalBufferPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalBufferInfo(payload, handles, *pExternalBufferInfo);
    payload.write_pod(pExternalBufferProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalBufferProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalBufferProperties(r, handles, pExternalBufferProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceExternalFencePropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalFencePropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalFenceInfo(payload, handles, *pExternalFenceInfo);
    payload.write_pod(pExternalFenceProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalFenceProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalFenceProperties(r, handles, pExternalFenceProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    serialize_VkPhysicalDeviceExternalSemaphoreInfo(payload, handles, *pExternalSemaphoreInfo);
    payload.write_pod(pExternalSemaphoreProperties->sType);
    serialize_pnext_chain(payload, handles, pExternalSemaphoreProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkExternalSemaphoreProperties(r, handles, pExternalSemaphoreProperties, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetSemaphoreCounterValueKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    AV_SCOPE("av_vkGetSemaphoreCounterValueKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphore(payload, handles, semaphore);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkQueueSubmit2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    AV_SCOPE("av_vkQueueSubmit2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkQueue(payload, handles, queue);
    payload.write_pod(submitCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(submitCount); ++av_i) {
        serialize_VkSubmitInfo2(payload, handles, pSubmits[av_i]);
    }
    serialize_VkFence(payload, handles, fence);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkSignalSemaphoreKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    AV_SCOPE("av_vkSignalSemaphoreKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreSignalInfo(payload, handles, *pSignalInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkTrimCommandPoolKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    AV_SCOPE("av_vkTrimCommandPoolKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCommandPool(payload, handles, commandPool);
    payload.write_pod(flags);
    if (t.send_async(::av::vkproto::kCmd_vkTrimCommandPoolKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkUpdateDescriptorSetWithTemplateKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    AV_SCOPE("av_vkUpdateDescriptorSetWithTemplateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDescriptorSet(payload, handles, descriptorSet);
    serialize_VkDescriptorUpdateTemplate(payload, handles, descriptorUpdateTemplate);
    const uint64_t av_blob = (pData != nullptr) ? handles.template_bytes(descriptorUpdateTemplate) : 0;
    payload.write_varint(av_blob);
    if (av_blob != 0) { payload.write(pData, static_cast<size_t>(av_blob)); }
    if (t.send_async(::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkWaitSemaphoresKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    AV_SCOPE("av_vkWaitSemaphoresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreWaitInfo(payload, handles, *pWaitInfo);
    payload.write_pod(timeout);
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdBeginConditionalRenderingEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    AV_SCOPE("av_vkCmdBeginConditionalRenderingEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkConditionalRenderingBeginInfoEXT(payload, handles, *pConditionalRenderingBegin);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndConditionalRenderingEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer) {
    AV_SCOPE("av_vkCmdEndConditionalRenderingEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetColorWriteEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    AV_SCOPE("av_vkCmdSetColorWriteEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        payload.write_pod(pColorWriteEnables[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBiasEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    AV_SCOPE("av_vkCmdSetDepthBiasEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthBiasEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLogicOpEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    AV_SCOPE("av_vkCmdSetLogicOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(logicOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPatchControlPointsEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    AV_SCOPE("av_vkCmdSetPatchControlPointsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(patchControlPoints);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPrimitiveRestartEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    AV_SCOPE("av_vkCmdSetPrimitiveRestartEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(primitiveRestartEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRasterizerDiscardEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    AV_SCOPE("av_vkCmdSetRasterizerDiscardEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(rasterizerDiscardEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetVertexInputEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    AV_SCOPE("av_vkCmdSetVertexInputEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(vertexBindingDescriptionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(vertexBindingDescriptionCount); ++av_i) {
        serialize_VkVertexInputBindingDescription2EXT(payload, handles, pVertexBindingDescriptions[av_i]);
    }
    payload.write_pod(vertexAttributeDescriptionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(vertexAttributeDescriptionCount); ++av_i) {
        serialize_VkVertexInputAttributeDescription2EXT(payload, handles, pVertexAttributeDescriptions[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetVertexInputEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkBuildAccelerationStructuresKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("av_vkBuildAccelerationStructuresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, deferredOperation);
    payload.write_pod(infoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, handles, pInfos[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        const uint32_t av_n = static_cast<uint32_t>(pInfos[av_i].geometryCount);
        for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[av_i][av_j]);
        }
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdBindShadersEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) {
    AV_SCOPE("av_vkCmdBindShadersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stageCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(stageCount); ++av_i) {
        payload.write_pod(pStages[av_i]);
    }
    const uint8_t pShaders_present = pShaders != nullptr ? 1 : 0;
    payload.write_pod(pShaders_present);
    if (pShaders_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(stageCount); ++av_i) {
            serialize_VkShaderEXT(payload, handles, pShaders[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindShadersEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindVertexBuffers2EXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    AV_SCOPE("av_vkCmdBindVertexBuffers2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pBuffers[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        payload.write_pod(pOffsets[av_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
            payload.write_pod(pSizes[av_i]);
        }
    }
    const uint8_t pStrides_present = pStrides != nullptr ? 1 : 0;
    payload.write_pod(pStrides_present);
    if (pStrides_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
            payload.write_pod(pStrides[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBuildAccelerationStructuresIndirectKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) {
    AV_SCOPE("av_vkCmdBuildAccelerationStructuresIndirectKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(infoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, handles, pInfos[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        payload.write_pod(pIndirectDeviceAddresses[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        payload.write_pod(pIndirectStrides[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        const uint32_t av_n = static_cast<uint32_t>(pInfos[av_i].geometryCount);
        for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
            payload.write_pod(ppMaxPrimitiveCounts[av_i][av_j]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBuildAccelerationStructuresKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
    AV_SCOPE("av_vkCmdBuildAccelerationStructuresKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(infoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, handles, pInfos[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(infoCount); ++av_i) {
        const uint32_t av_n = static_cast<uint32_t>(pInfos[av_i].geometryCount);
        for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
            serialize_VkAccelerationStructureBuildRangeInfoKHR(payload, ppBuildRangeInfos[av_i][av_j]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("av_vkCmdCopyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, handles, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("av_vkCmdCopyAccelerationStructureToMemoryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, handles, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("av_vkCmdCopyMemoryToAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, handles, *pInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawMeshTasksEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    AV_SCOPE("av_vkCmdDrawMeshTasksEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(groupCountX);
    payload.write_pod(groupCountY);
    payload.write_pod(groupCountZ);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawMeshTasksIndirectCountEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawMeshTasksIndirectCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    serialize_VkBuffer(payload, handles, countBuffer);
    payload.write_pod(countBufferOffset);
    payload.write_pod(maxDrawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawMeshTasksIndirectEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawMeshTasksIndirectEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    payload.write_pod(drawCount);
    payload.write_pod(stride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushDescriptorSetKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    AV_SCOPE("av_vkCmdPushDescriptorSetKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineBindPoint);
    serialize_VkPipelineLayout(payload, handles, layout);
    payload.write_pod(set);
    payload.write_pod(descriptorWriteCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(descriptorWriteCount); ++av_i) {
        serialize_VkWriteDescriptorSet(payload, handles, pDescriptorWrites[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetAlphaToCoverageEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    AV_SCOPE("av_vkCmdSetAlphaToCoverageEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(alphaToCoverageEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetAlphaToOneEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    AV_SCOPE("av_vkCmdSetAlphaToOneEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(alphaToOneEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetColorBlendAdvancedEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    AV_SCOPE("av_vkCmdSetColorBlendAdvancedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        serialize_VkColorBlendAdvancedEXT(payload, pColorBlendAdvanced[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetColorBlendEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    AV_SCOPE("av_vkCmdSetColorBlendEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        payload.write_pod(pColorBlendEnables[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetColorBlendEquationEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    AV_SCOPE("av_vkCmdSetColorBlendEquationEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        serialize_VkColorBlendEquationEXT(payload, pColorBlendEquations[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetColorWriteMaskEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    AV_SCOPE("av_vkCmdSetColorWriteMaskEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstAttachment);
    payload.write_pod(attachmentCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(attachmentCount); ++av_i) {
        payload.write_pod(pColorWriteMasks[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetConservativeRasterizationModeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    AV_SCOPE("av_vkCmdSetConservativeRasterizationModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(conservativeRasterizationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageModulationModeNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    AV_SCOPE("av_vkCmdSetCoverageModulationModeNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageModulationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageModulationTableEnableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    AV_SCOPE("av_vkCmdSetCoverageModulationTableEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageModulationTableEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageModulationTableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    AV_SCOPE("av_vkCmdSetCoverageModulationTableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageModulationTableCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(coverageModulationTableCount); ++av_i) {
        payload.write_pod(pCoverageModulationTable[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageReductionModeNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    AV_SCOPE("av_vkCmdSetCoverageReductionModeNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageReductionMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageToColorEnableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    AV_SCOPE("av_vkCmdSetCoverageToColorEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageToColorEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCoverageToColorLocationNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    AV_SCOPE("av_vkCmdSetCoverageToColorLocationNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(coverageToColorLocation);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetCullModeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    AV_SCOPE("av_vkCmdSetCullModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(cullMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetCullModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBoundsTestEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    AV_SCOPE("av_vkCmdSetDepthBoundsTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthBoundsTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthClampEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    AV_SCOPE("av_vkCmdSetDepthClampEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthClampEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthClampRangeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) {
    AV_SCOPE("av_vkCmdSetDepthClampRangeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthClampMode);
    serialize_VkDepthClampRangeEXT(payload, *pDepthClampRange);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthClipEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    AV_SCOPE("av_vkCmdSetDepthClipEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthClipEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthClipNegativeOneToOneEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    AV_SCOPE("av_vkCmdSetDepthClipNegativeOneToOneEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(negativeOneToOne);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthCompareOpEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    AV_SCOPE("av_vkCmdSetDepthCompareOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthCompareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthTestEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    AV_SCOPE("av_vkCmdSetDepthTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthWriteEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    AV_SCOPE("av_vkCmdSetDepthWriteEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(depthWriteEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetExtraPrimitiveOverestimationSizeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    AV_SCOPE("av_vkCmdSetExtraPrimitiveOverestimationSizeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(extraPrimitiveOverestimationSize);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetFragmentShadingRateKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) {
    AV_SCOPE("av_vkCmdSetFragmentShadingRateKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkExtent2D(payload, *pFragmentSize);
    for (uint32_t av_i = 0; av_i < 2u; ++av_i) {
        payload.write_pod(combinerOps[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetFrontFaceEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    AV_SCOPE("av_vkCmdSetFrontFaceEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(frontFace);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetFrontFaceEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLineRasterizationModeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    AV_SCOPE("av_vkCmdSetLineRasterizationModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(lineRasterizationMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLineStippleEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    AV_SCOPE("av_vkCmdSetLineStippleEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(lineStippleFactor);
    payload.write_pod(lineStipplePattern);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLineStippleEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    AV_SCOPE("av_vkCmdSetLineStippleEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stippledLineEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetLogicOpEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    AV_SCOPE("av_vkCmdSetLogicOpEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(logicOpEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPolygonModeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    AV_SCOPE("av_vkCmdSetPolygonModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(polygonMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPolygonModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetPrimitiveTopologyEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    AV_SCOPE("av_vkCmdSetPrimitiveTopologyEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(primitiveTopology);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetProvokingVertexModeEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    AV_SCOPE("av_vkCmdSetProvokingVertexModeEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(provokingVertexMode);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRasterizationSamplesEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    AV_SCOPE("av_vkCmdSetRasterizationSamplesEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(rasterizationSamples);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRasterizationStreamEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    AV_SCOPE("av_vkCmdSetRasterizationStreamEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(rasterizationStream);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRayTracingPipelineStackSizeKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) {
    AV_SCOPE("av_vkCmdSetRayTracingPipelineStackSizeKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineStackSize);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRepresentativeFragmentTestEnableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    AV_SCOPE("av_vkCmdSetRepresentativeFragmentTestEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(representativeFragmentTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetSampleLocationsEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    AV_SCOPE("av_vkCmdSetSampleLocationsEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(sampleLocationsEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetSampleMaskEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    AV_SCOPE("av_vkCmdSetSampleMaskEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(samples);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>((samples + 31) / 32); ++av_i) {
        payload.write_pod(pSampleMask[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetSampleMaskEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetScissorWithCountEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    AV_SCOPE("av_vkCmdSetScissorWithCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(scissorCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(scissorCount); ++av_i) {
        serialize_VkRect2D(payload, pScissors[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetShadingRateImageEnableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    AV_SCOPE("av_vkCmdSetShadingRateImageEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(shadingRateImageEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilOpEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    AV_SCOPE("av_vkCmdSetStencilOpEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(faceMask);
    payload.write_pod(failOp);
    payload.write_pod(passOp);
    payload.write_pod(depthFailOp);
    payload.write_pod(compareOp);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilOpEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetStencilTestEnableEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    AV_SCOPE("av_vkCmdSetStencilTestEnableEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stencilTestEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetTessellationDomainOriginEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    AV_SCOPE("av_vkCmdSetTessellationDomainOriginEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(domainOrigin);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetViewportSwizzleNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    AV_SCOPE("av_vkCmdSetViewportSwizzleNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstViewport);
    payload.write_pod(viewportCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(viewportCount); ++av_i) {
        serialize_VkViewportSwizzleNV(payload, pViewportSwizzles[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetViewportWScalingEnableNV(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    AV_SCOPE("av_vkCmdSetViewportWScalingEnableNV");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(viewportWScalingEnable);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetViewportWithCountEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    AV_SCOPE("av_vkCmdSetViewportWithCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(viewportCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(viewportCount); ++av_i) {
        serialize_VkViewport(payload, pViewports[av_i]);
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdTraceRaysIndirectKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) {
    AV_SCOPE("av_vkCmdTraceRaysIndirectKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pRaygenShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pMissShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pHitShaderBindingTable);
    serialize_VkStridedDeviceAddressRegionKHR(payload, *pCallableShaderBindingTable);
    payload.write_pod(indirectDeviceAddress);
    if (t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdTraceRaysKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) {
    AV_SCOPE("av_vkCmdTraceRaysKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
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

void av_vkCmdWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
    AV_SCOPE("av_vkCmdWriteAccelerationStructuresPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(accelerationStructureCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(accelerationStructureCount); ++av_i) {
        serialize_VkAccelerationStructureKHR(payload, handles, pAccelerationStructures[av_i]);
    }
    payload.write_pod(queryType);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(firstQuery);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCopyAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("av_vkCopyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, deferredOperation);
    serialize_VkCopyAccelerationStructureInfoKHR(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCopyAccelerationStructureToMemoryKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) {
    AV_SCOPE("av_vkCopyAccelerationStructureToMemoryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, deferredOperation);
    serialize_VkCopyAccelerationStructureToMemoryInfoKHR(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCopyImageToImageEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) {
    AV_SCOPE("av_vkCopyImageToImageEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCopyImageToImageInfo(payload, handles, *pCopyImageToImageInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCopyImageToMemoryEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) {
    AV_SCOPE("av_vkCopyImageToMemoryEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCopyImageToMemoryInfo(payload, handles, *pCopyImageToMemoryInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCopyMemoryToAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) {
    AV_SCOPE("av_vkCopyMemoryToAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, deferredOperation);
    serialize_VkCopyMemoryToAccelerationStructureInfoKHR(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCopyMemoryToImageEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) {
    AV_SCOPE("av_vkCopyMemoryToImageEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkCopyMemoryToImageInfo(payload, handles, *pCopyMemoryToImageInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) {
    AV_SCOPE("av_vkCreateAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAccelerationStructureCreateInfoKHR(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateDeferredOperationKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) {
    AV_SCOPE("av_vkCreateDeferredOperationKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkCreateRayTracingPipelinesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    AV_SCOPE("av_vkCreateRayTracingPipelinesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, deferredOperation);
    serialize_VkPipelineCache(payload, handles, pipelineCache);
    payload.write_pod(createInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(createInfoCount); ++av_i) {
        serialize_VkRayTracingPipelineCreateInfoKHR(payload, handles, pCreateInfos[av_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(createInfoCount); ++av_k) {
        if (!r.read_pod(pPipelines[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkCreateShadersEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) {
    AV_SCOPE("av_vkCreateShadersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(createInfoCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(createInfoCount); ++av_i) {
        serialize_VkShaderCreateInfoEXT(payload, handles, pCreateInfos[av_i]);
    }
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreateShadersEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(createInfoCount); ++av_k) {
        if (!r.read_pod(pShaders[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkDeferredOperationJoinKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("av_vkDeferredOperationJoinKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, operation);
    const ::av::SyncCallScope av_call{t};
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

void av_vkDestroyAccelerationStructureKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyAccelerationStructureKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAccelerationStructureKHR(payload, handles, accelerationStructure);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyDeferredOperationKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyDeferredOperationKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, operation);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyDeferredOperationKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkDestroyShaderEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyShaderEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderEXT(payload, handles, shader);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyShaderEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkGetAccelerationStructureBuildSizesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) {
    AV_SCOPE("av_vkGetAccelerationStructureBuildSizesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(buildType);
    serialize_VkAccelerationStructureBuildGeometryInfoKHR(payload, handles, *pBuildInfo);
    const uint8_t pMaxPrimitiveCounts_present = pMaxPrimitiveCounts != nullptr ? 1 : 0;
    payload.write_pod(pMaxPrimitiveCounts_present);
    if (pMaxPrimitiveCounts_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(pBuildInfo->geometryCount); ++av_i) {
            payload.write_pod(pMaxPrimitiveCounts[av_i]);
        }
    }
    payload.write_pod(pSizeInfo->sType);
    serialize_pnext_chain(payload, handles, pSizeInfo->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkAccelerationStructureBuildSizesInfoKHR(r, handles, pSizeInfo, t.decode_arena())) {
        return;
    }
}

VkDeviceAddress av_vkGetAccelerationStructureDeviceAddressKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) {
    AV_SCOPE("av_vkGetAccelerationStructureDeviceAddressKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAccelerationStructureDeviceAddressInfoKHR(payload, handles, *pInfo);
    const ::av::SyncCallScope av_call{t};
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

uint32_t av_vkGetDeferredOperationMaxConcurrencyKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("av_vkGetDeferredOperationMaxConcurrencyKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, operation);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetDeferredOperationResultKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeferredOperationKHR operation) {
    AV_SCOPE("av_vkGetDeferredOperationResultKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeferredOperationKHR(payload, handles, operation);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetDeviceAccelerationStructureCompatibilityKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) {
    AV_SCOPE("av_vkGetDeviceAccelerationStructureCompatibilityKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAccelerationStructureVersionInfoKHR(payload, handles, *pVersionInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkGetImageSubresourceLayout2EXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("av_vkGetImageSubresourceLayout2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    serialize_VkImageSubresource2(payload, handles, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, handles, pLayout->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, handles, pLayout, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetPhysicalDeviceFragmentShadingRatesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) {
    AV_SCOPE("av_vkGetPhysicalDeviceFragmentShadingRatesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pFragmentShadingRates != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pFragmentShadingRateCount);
    }
    const ::av::SyncCallScope av_call{t};
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
            if (!deserialize_VkPhysicalDeviceFragmentShadingRateKHR(r, handles, &pFragmentShadingRates[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pFragmentShadingRateCount = pFragmentShadingRateCount_answered;
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("av_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipeline(payload, handles, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<uint64_t>(dataSize);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetRayTracingShaderGroupHandlesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) {
    AV_SCOPE("av_vkGetRayTracingShaderGroupHandlesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipeline(payload, handles, pipeline);
    payload.write_pod(firstGroup);
    payload.write_pod(groupCount);
    payload.write_pod<uint64_t>(dataSize);
    const ::av::SyncCallScope av_call{t};
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

VkDeviceSize av_vkGetRayTracingShaderGroupStackSizeKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) {
    AV_SCOPE("av_vkGetRayTracingShaderGroupStackSizeKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipeline(payload, handles, pipeline);
    payload.write_pod(group);
    payload.write_pod(groupShader);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetShaderBinaryDataEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkShaderEXT shader, size_t* pDataSize, void* pData) {
    AV_SCOPE("av_vkGetShaderBinaryDataEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderEXT(payload, handles, shader);
    const uint8_t array_present = pData != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint64_t>(*pDataSize);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetShaderBinaryDataEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint64_t pDataSize_answered = 0;
    if (!r.read_pod(pDataSize_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present && (pDataSize_answered > *pDataSize ||
        (pDataSize_answered != 0 && !r.read(pData, static_cast<size_t>(pDataSize_answered))))) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pDataSize_answered > static_cast<uint64_t>(~size_t{0})) {
        return VK_ERROR_DEVICE_LOST;
    }
    *pDataSize = static_cast<size_t>(pDataSize_answered);
    return static_cast<VkResult>(result_code);
}

VkResult av_vkTransitionImageLayoutEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) {
    AV_SCOPE("av_vkTransitionImageLayoutEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(transitionCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(transitionCount); ++av_i) {
        serialize_VkHostImageLayoutTransitionInfo(payload, handles, pTransitions[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkWriteAccelerationStructuresPropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) {
    AV_SCOPE("av_vkWriteAccelerationStructuresPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(accelerationStructureCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(accelerationStructureCount); ++av_i) {
        serialize_VkAccelerationStructureKHR(payload, handles, pAccelerationStructures[av_i]);
    }
    payload.write_pod(queryType);
    payload.write_pod<uint64_t>(dataSize);
    payload.write_pod<uint64_t>(stride);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetCalibratedTimestampsEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    AV_SCOPE("av_vkGetCalibratedTimestampsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(timestampCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(timestampCount); ++av_i) {
        serialize_VkCalibratedTimestampInfoKHR(payload, handles, pTimestampInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(timestampCount); ++av_k) {
        if (!r.read_pod(pTimestamps[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    if (!r.read_pod(*pMaxDeviation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    AV_SCOPE("av_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pTimeDomains != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pTimeDomainCount);
    }
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdSetRenderingAttachmentLocationsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) {
    AV_SCOPE("av_vkCmdSetRenderingAttachmentLocationsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderingAttachmentLocationInfo(payload, handles, *pLocationInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetRenderingInputAttachmentIndicesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) {
    AV_SCOPE("av_vkCmdSetRenderingInputAttachmentIndicesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkRenderingInputAttachmentIndexInfo(payload, handles, *pInputAttachmentIndexInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindIndexBuffer2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) {
    AV_SCOPE("av_vkCmdBindIndexBuffer2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBuffer(payload, handles, buffer);
    payload.write_pod(offset);
    payload.write_pod(size);
    payload.write_pod(indexType);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkCreatePipelineBinariesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) {
    AV_SCOPE("av_vkCreatePipelineBinariesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineBinaryCreateInfoKHR(payload, handles, *pCreateInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    payload.write_pod(pBinaries->sType);
    serialize_pnext_chain(payload, handles, pBinaries->pNext);
    payload.write_pod(pBinaries->pipelineBinaryCount);
    payload.write_pod<uint8_t>(pBinaries->pPipelineBinaries != nullptr ? 1 : 0);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkCreatePipelineBinariesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkPipelineBinaryHandlesInfoKHR(r, handles, pBinaries, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void av_vkDestroyPipelineBinaryKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkDestroyPipelineBinaryKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineBinaryKHR(payload, handles, pipelineBinary);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    if (t.send_async(::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkGetDeviceImageSubresourceLayoutKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("av_vkGetDeviceImageSubresourceLayoutKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceImageSubresourceInfo(payload, handles, *pInfo);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, handles, pLayout->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, handles, pLayout, t.decode_arena())) {
        return;
    }
}

void av_vkGetImageSubresourceLayout2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) {
    AV_SCOPE("av_vkGetImageSubresourceLayout2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImage(payload, handles, image);
    serialize_VkImageSubresource2(payload, handles, *pSubresource);
    payload.write_pod(pLayout->sType);
    serialize_pnext_chain(payload, handles, pLayout->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkSubresourceLayout2(r, handles, pLayout, t.decode_arena())) {
        return;
    }
}

VkResult av_vkGetPipelineBinaryDataKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPipelineBinaryDataInfoKHR* pInfo, VkPipelineBinaryKeyKHR* pPipelineBinaryKey, size_t* pPipelineBinaryDataSize, void* pPipelineBinaryData) {
    AV_SCOPE("av_vkGetPipelineBinaryDataKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineBinaryDataInfoKHR(payload, handles, *pInfo);
    payload.write_pod(pPipelineBinaryKey->sType);
    serialize_pnext_chain(payload, handles, pPipelineBinaryKey->pNext);
    const uint8_t array_present = pPipelineBinaryData != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint64_t>(*pPipelineBinaryDataSize);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineBinaryDataKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkPipelineBinaryKeyKHR(r, handles, pPipelineBinaryKey, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    uint64_t pPipelineBinaryDataSize_answered = 0;
    if (!r.read_pod(pPipelineBinaryDataSize_answered)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (array_present && (pPipelineBinaryDataSize_answered > *pPipelineBinaryDataSize ||
        (pPipelineBinaryDataSize_answered != 0 && !r.read(pPipelineBinaryData, static_cast<size_t>(pPipelineBinaryDataSize_answered))))) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (pPipelineBinaryDataSize_answered > static_cast<uint64_t>(~size_t{0})) {
        return VK_ERROR_DEVICE_LOST;
    }
    *pPipelineBinaryDataSize = static_cast<size_t>(pPipelineBinaryDataSize_answered);
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetPipelineKeyKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) {
    AV_SCOPE("av_vkGetPipelineKeyKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkPipelineCreateInfoKHR(payload, handles, *pPipelineCreateInfo);
    payload.write_pod(pPipelineKey->sType);
    serialize_pnext_chain(payload, handles, pPipelineKey->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPipelineKeyKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkPipelineBinaryKeyKHR(r, handles, pPipelineKey, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

void av_vkGetRenderingAreaGranularityKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) {
    AV_SCOPE("av_vkGetRenderingAreaGranularityKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkRenderingAreaInfo(payload, handles, *pRenderingAreaInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkReleaseCapturedPipelineDataKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) {
    AV_SCOPE("av_vkReleaseCapturedPipelineDataKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkReleaseCapturedPipelineDataInfoKHR(payload, handles, *pInfo);
    (void)pAllocator;  // VkAllocationCallbacks not crossed over the wire
    const ::av::SyncCallScope av_call{t};
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

void av_vkCmdBeginQueryIndexedEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    AV_SCOPE("av_vkCmdBeginQueryIndexedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    payload.write_pod(flags);
    payload.write_pod(index);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBeginTransformFeedbackEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("av_vkCmdBeginTransformFeedbackEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(counterBufferCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pCounterBuffers[av_i]);
    }
    const uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(counterBufferCount); ++av_i) {
            payload.write_pod(pCounterBufferOffsets[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindTransformFeedbackBuffersEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    AV_SCOPE("av_vkCmdBindTransformFeedbackBuffersEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstBinding);
    payload.write_pod(bindingCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pBuffers[av_i]);
    }
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
        payload.write_pod(pOffsets[av_i]);
    }
    const uint8_t pSizes_present = pSizes != nullptr ? 1 : 0;
    payload.write_pod(pSizes_present);
    if (pSizes_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(bindingCount); ++av_i) {
            payload.write_pod(pSizes[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawIndirectByteCountEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    AV_SCOPE("av_vkCmdDrawIndirectByteCountEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    serialize_VkBuffer(payload, handles, counterBuffer);
    payload.write_pod(counterBufferOffset);
    payload.write_pod(counterOffset);
    payload.write_pod(vertexStride);
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndQueryIndexedEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    AV_SCOPE("av_vkCmdEndQueryIndexedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkQueryPool(payload, handles, queryPool);
    payload.write_pod(query);
    payload.write_pod(index);
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdEndTransformFeedbackEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    AV_SCOPE("av_vkCmdEndTransformFeedbackEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(firstCounterBuffer);
    payload.write_pod(counterBufferCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(counterBufferCount); ++av_i) {
        serialize_VkBuffer(payload, handles, pCounterBuffers[av_i]);
    }
    const uint8_t pCounterBufferOffsets_present = pCounterBufferOffsets != nullptr ? 1 : 0;
    payload.write_pod(pCounterBufferOffsets_present);
    if (pCounterBufferOffsets_present) {
        for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(counterBufferCount); ++av_i) {
            payload.write_pod(pCounterBufferOffsets[av_i]);
        }
    }
    if (t.send_async(::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkSetDeviceMemoryPriorityEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkDeviceMemory memory, float priority) {
    AV_SCOPE("av_vkSetDeviceMemoryPriorityEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkDeviceMemory(payload, handles, memory);
    payload.write_pod(priority);
    if (t.send_async(::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkGetFenceFdKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("av_vkGetFenceFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkFenceGetFdInfoKHR(payload, handles, *pGetFdInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetMemoryFdKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("av_vkGetMemoryFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkMemoryGetFdInfoKHR(payload, handles, *pGetFdInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkGetMemoryFdPropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    AV_SCOPE("av_vkGetMemoryFdPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(handleType);
    payload.write_pod(fd);
    payload.write_pod(pMemoryFdProperties->sType);
    serialize_pnext_chain(payload, handles, pMemoryFdProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    if (!deserialize_VkMemoryFdPropertiesKHR(r, handles, pMemoryFdProperties, t.decode_arena())) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetSemaphoreFdKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    AV_SCOPE("av_vkGetSemaphoreFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkSemaphoreGetFdInfoKHR(payload, handles, *pGetFdInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkImportFenceFdKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    AV_SCOPE("av_vkImportFenceFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImportFenceFdInfoKHR(payload, handles, *pImportFenceFdInfo);
    const ::av::SyncCallScope av_call{t};
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

VkResult av_vkImportSemaphoreFdKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    AV_SCOPE("av_vkImportSemaphoreFdKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkImportSemaphoreFdInfoKHR(payload, handles, *pImportSemaphoreFdInfo);
    const ::av::SyncCallScope av_call{t};
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

void av_vkAntiLagUpdateAMD(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkAntiLagDataAMD* pData) {
    AV_SCOPE("av_vkAntiLagUpdateAMD");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkAntiLagDataAMD(payload, handles, *pData);
    if (t.send_async(::av::vkproto::kCmd_vkAntiLagUpdateAMD, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT* pBindDescriptorBufferEmbeddedSamplersInfo) {
    AV_SCOPE("av_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBindDescriptorBufferEmbeddedSamplersInfoEXT(payload, handles, *pBindDescriptorBufferEmbeddedSamplersInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdBindDescriptorSets2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo* pBindDescriptorSetsInfo) {
    AV_SCOPE("av_vkCmdBindDescriptorSets2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkBindDescriptorSetsInfo(payload, handles, *pBindDescriptorSetsInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdBindDescriptorSets2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawMultiEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride) {
    AV_SCOPE("av_vkCmdDrawMultiEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(drawCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(drawCount); ++av_i) {
        serialize_VkMultiDrawInfoEXT(payload, (*reinterpret_cast<const VkMultiDrawInfoEXT*>(reinterpret_cast<const uint8_t*>(pVertexInfo) + static_cast<size_t>(av_i) * stride)));
    }
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    (void)stride;  // the array is sent packed
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMultiEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdDrawMultiIndexedEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t* pVertexOffset) {
    AV_SCOPE("av_vkCmdDrawMultiIndexedEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(drawCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(drawCount); ++av_i) {
        serialize_VkMultiDrawIndexedInfoEXT(payload, (*reinterpret_cast<const VkMultiDrawIndexedInfoEXT*>(reinterpret_cast<const uint8_t*>(pIndexInfo) + static_cast<size_t>(av_i) * stride)));
    }
    payload.write_pod(instanceCount);
    payload.write_pod(firstInstance);
    (void)stride;  // the array is sent packed
    payload.write_pod<uint8_t>(pVertexOffset != nullptr ? 1 : 0);
    if (pVertexOffset != nullptr) { payload.write_pod(*pVertexOffset); }
    if (t.send_async(::av::vkproto::kCmd_vkCmdDrawMultiIndexedEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushConstants2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkPushConstantsInfo* pPushConstantsInfo) {
    AV_SCOPE("av_vkCmdPushConstants2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkPushConstantsInfo(payload, handles, *pPushConstantsInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushConstants2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushDescriptorSet2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo* pPushDescriptorSetInfo) {
    AV_SCOPE("av_vkCmdPushDescriptorSet2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkPushDescriptorSetInfo(payload, handles, *pPushDescriptorSetInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSet2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdPushDescriptorSetWithTemplate2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo) {
    AV_SCOPE("av_vkCmdPushDescriptorSetWithTemplate2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkPushDescriptorSetWithTemplateInfo(payload, handles, *pPushDescriptorSetWithTemplateInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdPushDescriptorSetWithTemplate2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDepthBias2EXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkDepthBiasInfoEXT* pDepthBiasInfo) {
    AV_SCOPE("av_vkCmdSetDepthBias2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkDepthBiasInfoEXT(payload, handles, *pDepthBiasInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDepthBias2EXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetDescriptorBufferOffsets2EXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT* pSetDescriptorBufferOffsetsInfo) {
    AV_SCOPE("av_vkCmdSetDescriptorBufferOffsets2EXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSetDescriptorBufferOffsetsInfoEXT(payload, handles, *pSetDescriptorBufferOffsetsInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetDescriptorBufferOffsets2EXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdSetSampleLocationsEXT(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) {
    AV_SCOPE("av_vkCmdSetSampleLocationsEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    serialize_VkSampleLocationsInfoEXT(payload, handles, *pSampleLocationsInfo);
    if (t.send_async(::av::vkproto::kCmd_vkCmdSetSampleLocationsEXT, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdTraceRaysIndirect2KHR(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress) {
    AV_SCOPE("av_vkCmdTraceRaysIndirect2KHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(indirectDeviceAddress);
    if (t.send_async(::av::vkproto::kCmd_vkCmdTraceRaysIndirect2KHR, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWriteBufferMarker2AMD(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) {
    AV_SCOPE("av_vkCmdWriteBufferMarker2AMD");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(stage);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(marker);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteBufferMarker2AMD, payload) != ::av::TransportResult::ok) {
        return;
    }
}

void av_vkCmdWriteBufferMarkerAMD(::av::ClientTransport& t, HandleTranslation& handles, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) {
    AV_SCOPE("av_vkCmdWriteBufferMarkerAMD");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkCommandBuffer(payload, handles, commandBuffer);
    payload.write_pod(pipelineStage);
    serialize_VkBuffer(payload, handles, dstBuffer);
    payload.write_pod(dstOffset);
    payload.write_pod(marker);
    if (t.send_async(::av::vkproto::kCmd_vkCmdWriteBufferMarkerAMD, payload) != ::av::TransportResult::ok) {
        return;
    }
}

VkResult av_vkGetCalibratedTimestampsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    AV_SCOPE("av_vkGetCalibratedTimestampsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    payload.write_pod(timestampCount);
    for (uint32_t av_i = 0; av_i < static_cast<uint32_t>(timestampCount); ++av_i) {
        serialize_VkCalibratedTimestampInfoKHR(payload, handles, pTimestampInfos[av_i]);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetCalibratedTimestampsKHR, payload, r) != ::av::TransportResult::ok) {
        return VK_ERROR_DEVICE_LOST;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return VK_ERROR_DEVICE_LOST;
    }
    for (uint32_t av_k = 0; av_k < static_cast<uint32_t>(timestampCount); ++av_k) {
        if (!r.read_pod(pTimestamps[av_k])) {
        return VK_ERROR_DEVICE_LOST;
        }
    }
    if (!r.read_pod(*pMaxDeviation)) {
        return VK_ERROR_DEVICE_LOST;
    }
    return static_cast<VkResult>(result_code);
}

VkResult av_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    AV_SCOPE("av_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pTimeDomains != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pTimeDomainCount);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, payload, r) != ::av::TransportResult::ok) {
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

VkResult av_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR* pProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    const uint8_t array_present = pProperties != nullptr ? 1 : 0;
    payload.write_pod(array_present);
    if (array_present) {
        payload.write_pod<uint32_t>(*pPropertyCount);
    }
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR, payload, r) != ::av::TransportResult::ok) {
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
            if (!deserialize_VkCooperativeMatrixPropertiesKHR(r, handles, &pProperties[k], t.decode_arena())) {
            return VK_ERROR_DEVICE_LOST;
            }
        }
    }
    *pPropertyCount = pPropertyCount_answered;
    return static_cast<VkResult>(result_code);
}

void av_vkGetPhysicalDeviceMultisamplePropertiesEXT(::av::ClientTransport& t, HandleTranslation& handles, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) {
    AV_SCOPE("av_vkGetPhysicalDeviceMultisamplePropertiesEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkPhysicalDevice(payload, handles, physicalDevice);
    payload.write_pod(samples);
    payload.write_pod(pMultisampleProperties->sType);
    serialize_pnext_chain(payload, handles, pMultisampleProperties->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetPhysicalDeviceMultisamplePropertiesEXT, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkMultisamplePropertiesEXT(r, handles, pMultisampleProperties, t.decode_arena())) {
        return;
    }
}

void av_vkGetShaderModuleCreateInfoIdentifierEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier) {
    AV_SCOPE("av_vkGetShaderModuleCreateInfoIdentifierEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderModuleCreateInfo(payload, handles, *pCreateInfo);
    payload.write_pod(pIdentifier->sType);
    serialize_pnext_chain(payload, handles, pIdentifier->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetShaderModuleCreateInfoIdentifierEXT, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkShaderModuleIdentifierEXT(r, handles, pIdentifier, t.decode_arena())) {
        return;
    }
}

void av_vkGetShaderModuleIdentifierEXT(::av::ClientTransport& t, HandleTranslation& handles, VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier) {
    AV_SCOPE("av_vkGetShaderModuleIdentifierEXT");
    ::av::WireWriter payload = t.request_writer();
    serialize_VkDevice(payload, handles, device);
    serialize_VkShaderModule(payload, handles, shaderModule);
    payload.write_pod(pIdentifier->sType);
    serialize_pnext_chain(payload, handles, pIdentifier->pNext);
    const ::av::SyncCallScope av_call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(::av::vkproto::kCmd_vkGetShaderModuleIdentifierEXT, payload, r) != ::av::TransportResult::ok) {
        return;
    }
    int32_t result_code = 0;
    if (!r.read_pod(result_code)) {
        return;
    }
    if (!deserialize_VkShaderModuleIdentifierEXT(r, handles, pIdentifier, t.decode_arena())) {
        return;
    }
}

}  // namespace av::vkproto
