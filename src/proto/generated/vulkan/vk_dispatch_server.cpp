// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_dispatch_server.h"

#include "core/trace/function_scope.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    switch (cmd_id) {
    case ::av::vkproto::kCmd_vkDestroyInstance: {
        AV_SCOPE("server:vkDestroyInstance");
        VkInstance instance = VK_NULL_HANDLE;
        if (!deserialize_VkInstance(r, instance)) return false;
        s.on_vkDestroyInstance(instance, nullptr);
        handle_ended(as_number(instance));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDevice: {
        AV_SCOPE("server:vkDestroyDevice");
        VkDevice device = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        s.on_vkDestroyDevice(device, nullptr);
        handle_ended(as_number(device));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceQueue: {
        AV_SCOPE("server:vkGetDeviceQueue");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex{};
        uint32_t queueIndex{};
        VkQueue pQueue_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(queueFamilyIndex)) return false;
        if (!r.read_pod(queueIndex)) return false;
        if (handles_room(static_cast<uint64_t>(1u))) s.on_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &pQueue_out);
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(0);
        serialize_VkQueue(reply, pQueue_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDevices: {
        AV_SCOPE("server:vkEnumeratePhysicalDevices");
        VkInstance instance = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pPhysicalDeviceCount_local = 0;
        VkPhysicalDevice* pPhysicalDevices_storage = nullptr;
        size_t pPhysicalDevices_storage_count = 0;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceCount_local)) return false;
            pPhysicalDevices_storage_count = static_cast<size_t>(pPhysicalDeviceCount_local);
            if (pPhysicalDevices_storage_count != 0) {
                pPhysicalDevices_storage = arena.allocate<VkPhysicalDevice>(pPhysicalDevices_storage_count);
                if (pPhysicalDevices_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pPhysicalDevices_storage_count; ++av_z) { pPhysicalDevices_storage[av_z] = {}; }
            }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPhysicalDevices_storage_count)) ? s.on_vkEnumeratePhysicalDevices(instance, &pPhysicalDeviceCount_local, array_present ? pPhysicalDevices_storage : nullptr) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(instance));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPhysicalDeviceCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPhysicalDeviceCount_local;
        if (array_present && pPhysicalDeviceCount_sent > pPhysicalDevices_storage_count) {
            pPhysicalDeviceCount_sent = static_cast<uint32_t>(pPhysicalDevices_storage_count);
        }
        reply.write_pod(pPhysicalDeviceCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPhysicalDeviceCount_sent; ++k) {
                serialize_VkPhysicalDevice(reply, pPhysicalDevices_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceProperties2(physicalDevice, &pProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceProperties2(reply, pProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateInstance: {
        AV_SCOPE("server:vkCreateInstance");
        VkInstanceCreateInfo pCreateInfo_local{};
        VkInstance pInstance_out = VK_NULL_HANDLE;
        if (!deserialize_VkInstanceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateInstance(&pCreateInfo_local, nullptr, &pInstance_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pInstance_out = VK_NULL_HANDLE;
        }
        serialize_VkInstance(reply, pInstance_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDevice: {
        AV_SCOPE("server:vkCreateDevice");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDeviceCreateInfo pCreateInfo_local{};
        VkDevice pDevice_out = VK_NULL_HANDLE;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkDeviceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDevice(physicalDevice, &pCreateInfo_local, nullptr, &pDevice_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(physicalDevice));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pDevice_out = VK_NULL_HANDLE;
        }
        serialize_VkDevice(reply, pDevice_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        s.on_vkGetPhysicalDeviceProperties(physicalDevice, &pProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceProperties(reply, pProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures: {
        AV_SCOPE("server:vkGetPhysicalDeviceFeatures");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        s.on_vkGetPhysicalDeviceFeatures(physicalDevice, &pFeatures_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceFeatures(reply, pFeatures_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceFormatProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        s.on_vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkFormatProperties(reply, pFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceImageFormatProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkImageType type{};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        VkImageCreateFlags flags{};
        VkImageFormatProperties pImageFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(type)) return false;
        if (!r.read_pod(tiling)) return false;
        if (!r.read_pod(usage)) return false;
        if (!r.read_pod(flags)) return false;
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, &pImageFormatProperties_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkImageFormatProperties pImageFormatProperties_blank{};
            pImageFormatProperties_out = pImageFormatProperties_blank;
        }
        serialize_VkImageFormatProperties(reply, pImageFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceMemoryProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        s.on_vkGetPhysicalDeviceMemoryProperties(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties(reply, pMemoryProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceQueueFamilyProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pQueueFamilyPropertyCount_local = 0;
        VkQueueFamilyProperties* pQueueFamilyProperties_storage = nullptr;
        size_t pQueueFamilyProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage_count = static_cast<size_t>(pQueueFamilyPropertyCount_local);
            if (pQueueFamilyProperties_storage_count != 0) {
                pQueueFamilyProperties_storage = arena.allocate<VkQueueFamilyProperties>(pQueueFamilyProperties_storage_count);
                if (pQueueFamilyProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pQueueFamilyProperties_storage_count; ++av_z) { pQueueFamilyProperties_storage[av_z] = {}; }
            }
        }
        s.on_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pQueueFamilyPropertyCount_sent = pQueueFamilyPropertyCount_local;
        if (array_present && pQueueFamilyPropertyCount_sent > pQueueFamilyProperties_storage_count) {
            pQueueFamilyPropertyCount_sent = static_cast<uint32_t>(pQueueFamilyProperties_storage_count);
        }
        reply.write_pod(pQueueFamilyPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pQueueFamilyPropertyCount_sent; ++k) {
                serialize_VkQueueFamilyProperties(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceSparseImageFormatProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkImageType type{};
        VkSampleCountFlagBits samples{};
        VkImageUsageFlags usage{};
        VkImageTiling tiling{};
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkSparseImageFormatProperties* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(type)) return false;
        if (!r.read_pod(samples)) return false;
        if (!r.read_pod(usage)) return false;
        if (!r.read_pod(tiling)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkSparseImageFormatProperties>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
        }
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pPropertyCount_sent = pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkSparseImageFormatProperties(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumerateDeviceExtensionProperties: {
        AV_SCOPE("server:vkEnumerateDeviceExtensionProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        const char* pLayerName = nullptr;
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkExtensionProperties* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        {
            uint64_t n = 0;
            if (!r.read_varint(n)) return false;
            if (n > 0) {
                char* tmp_str = arena.allocate_chars(static_cast<size_t>(n));
                if (tmp_str == nullptr) { return false; }
                if (!r.read(tmp_str, static_cast<size_t>(n))) return false;
                pLayerName = tmp_str;
            }
        }
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkExtensionProperties>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
        }
        VkResult result = s.on_vkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPropertyCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkExtensionProperties(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumerateDeviceLayerProperties: {
        AV_SCOPE("server:vkEnumerateDeviceLayerProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkLayerProperties* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkLayerProperties>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
        }
        VkResult result = s.on_vkEnumerateDeviceLayerProperties(physicalDevice, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPropertyCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkLayerProperties(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2: {
        AV_SCOPE("server:vkGetPhysicalDeviceFeatures2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures2 pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pFeatures_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFeatures_out), arena)) return false;
        s.on_vkGetPhysicalDeviceFeatures2(physicalDevice, &pFeatures_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceFeatures2(reply, pFeatures_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceFormatProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties2 pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(pFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFormatProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkFormatProperties2(reply, pFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceImageFormatProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceImageFormatInfo2 pImageFormatInfo_local{};
        VkImageFormatProperties2 pImageFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceImageFormatInfo2(r, &pImageFormatInfo_local, arena)) return false;
        if (!r.read_pod(pImageFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pImageFormatProperties_out), arena)) return false;
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &pImageFormatInfo_local, &pImageFormatProperties_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkImageFormatProperties2 pImageFormatProperties_blank{};
            pImageFormatProperties_blank.sType = pImageFormatProperties_out.sType;
            pImageFormatProperties_out = pImageFormatProperties_blank;
        }
        serialize_VkImageFormatProperties2(reply, pImageFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceMemoryProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties2 pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pMemoryProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties2(reply, pMemoryProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceQueueFamilyProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pQueueFamilyPropertyCount_local = 0;
        VkQueueFamilyProperties2* pQueueFamilyProperties_storage = nullptr;
        size_t pQueueFamilyProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage_count = static_cast<size_t>(pQueueFamilyPropertyCount_local);
            if (pQueueFamilyProperties_storage_count != 0) {
                pQueueFamilyProperties_storage = arena.allocate<VkQueueFamilyProperties2>(pQueueFamilyProperties_storage_count);
                if (pQueueFamilyProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pQueueFamilyProperties_storage_count; ++av_z) { pQueueFamilyProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pQueueFamilyProperties_storage_count; ++av_e) { pQueueFamilyProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; pQueueFamilyProperties_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pQueueFamilyPropertyCount_sent = pQueueFamilyPropertyCount_local;
        if (array_present && pQueueFamilyPropertyCount_sent > pQueueFamilyProperties_storage_count) {
            pQueueFamilyPropertyCount_sent = static_cast<uint32_t>(pQueueFamilyProperties_storage_count);
        }
        reply.write_pod(pQueueFamilyPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pQueueFamilyPropertyCount_sent; ++k) {
                serialize_VkQueueFamilyProperties2(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2: {
        AV_SCOPE("server:vkGetPhysicalDeviceSparseImageFormatProperties2");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceSparseImageFormatInfo2 pFormatInfo_local{};
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkSparseImageFormatProperties2* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceSparseImageFormatInfo2(r, &pFormatInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkSparseImageFormatProperties2>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pProperties_storage_count; ++av_e) { pProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2; pProperties_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &pFormatInfo_local, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pPropertyCount_sent = pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkSparseImageFormatProperties2(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkDeviceWaitIdle: {
        AV_SCOPE("server:vkDeviceWaitIdle");
        VkDevice device = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        VkResult result = s.on_vkDeviceWaitIdle(device);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueWaitIdle: {
        AV_SCOPE("server:vkQueueWaitIdle");
        VkQueue queue = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        VkResult result = s.on_vkQueueWaitIdle(queue);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateMemory: {
        AV_SCOPE("server:vkAllocateMemory");
        VkDevice device = VK_NULL_HANDLE;
        VkMemoryAllocateInfo pAllocateInfo_local{};
        VkDeviceMemory pMemory_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkMemoryAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkAllocateMemory(device, &pAllocateInfo_local, nullptr, &pMemory_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pMemory_out = VK_NULL_HANDLE;
        }
        serialize_VkDeviceMemory(reply, pMemory_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeMemory: {
        AV_SCOPE("server:vkFreeMemory");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        s.on_vkFreeMemory(device, memory, nullptr);
        handle_ended(as_number(memory));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory: {
        AV_SCOPE("server:vkBindBufferMemory");
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize memoryOffset{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (memory == VK_NULL_HANDLE) return false;
        if (!r.read_pod(memoryOffset)) return false;
        VkResult result = s.on_vkBindBufferMemory(device, buffer, memory, memoryOffset);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory: {
        AV_SCOPE("server:vkBindImageMemory");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize memoryOffset{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (memory == VK_NULL_HANDLE) return false;
        if (!r.read_pod(memoryOffset)) return false;
        VkResult result = s.on_vkBindImageMemory(device, image, memory, memoryOffset);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements: {
        AV_SCOPE("server:vkGetBufferMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkMemoryRequirements pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        s.on_vkGetBufferMemoryRequirements(device, buffer, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements: {
        AV_SCOPE("server:vkGetImageMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkMemoryRequirements pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        s.on_vkGetImageMemoryRequirements(device, image, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateBuffer: {
        AV_SCOPE("server:vkCreateBuffer");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferCreateInfo pCreateInfo_local{};
        VkBuffer pBuffer_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateBuffer(device, &pCreateInfo_local, nullptr, &pBuffer_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pBuffer_out = VK_NULL_HANDLE;
        }
        serialize_VkBuffer(reply, pBuffer_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyBuffer: {
        AV_SCOPE("server:vkDestroyBuffer");
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        s.on_vkDestroyBuffer(device, buffer, nullptr);
        handle_ended(as_number(buffer));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateImage: {
        AV_SCOPE("server:vkCreateImage");
        VkDevice device = VK_NULL_HANDLE;
        VkImageCreateInfo pCreateInfo_local{};
        VkImage pImage_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateImage(device, &pCreateInfo_local, nullptr, &pImage_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pImage_out = VK_NULL_HANDLE;
        }
        serialize_VkImage(reply, pImage_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyImage: {
        AV_SCOPE("server:vkDestroyImage");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        s.on_vkDestroyImage(device, image, nullptr);
        handle_ended(as_number(image));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateImageView: {
        AV_SCOPE("server:vkCreateImageView");
        VkDevice device = VK_NULL_HANDLE;
        VkImageViewCreateInfo pCreateInfo_local{};
        VkImageView pView_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageViewCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateImageView(device, &pCreateInfo_local, nullptr, &pView_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pView_out = VK_NULL_HANDLE;
        }
        serialize_VkImageView(reply, pView_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyImageView: {
        AV_SCOPE("server:vkDestroyImageView");
        VkDevice device = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageView(r, imageView)) return false;
        s.on_vkDestroyImageView(device, imageView, nullptr);
        handle_ended(as_number(imageView));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSampler: {
        AV_SCOPE("server:vkCreateSampler");
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerCreateInfo pCreateInfo_local{};
        VkSampler pSampler_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateSampler(device, &pCreateInfo_local, nullptr, &pSampler_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pSampler_out = VK_NULL_HANDLE;
        }
        serialize_VkSampler(reply, pSampler_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySampler: {
        AV_SCOPE("server:vkDestroySampler");
        VkDevice device = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSampler(r, sampler)) return false;
        s.on_vkDestroySampler(device, sampler, nullptr);
        handle_ended(as_number(sampler));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSemaphore: {
        AV_SCOPE("server:vkCreateSemaphore");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo pCreateInfo_local{};
        VkSemaphore pSemaphore_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateSemaphore(device, &pCreateInfo_local, nullptr, &pSemaphore_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pSemaphore_out = VK_NULL_HANDLE;
        }
        serialize_VkSemaphore(reply, pSemaphore_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySemaphore: {
        AV_SCOPE("server:vkDestroySemaphore");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        s.on_vkDestroySemaphore(device, semaphore, nullptr);
        handle_ended(as_number(semaphore));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateFence: {
        AV_SCOPE("server:vkCreateFence");
        VkDevice device = VK_NULL_HANDLE;
        VkFenceCreateInfo pCreateInfo_local{};
        VkFence pFence_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFenceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateFence(device, &pCreateInfo_local, nullptr, &pFence_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pFence_out = VK_NULL_HANDLE;
        }
        serialize_VkFence(reply, pFence_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyFence: {
        AV_SCOPE("server:vkDestroyFence");
        VkDevice device = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFence(r, fence)) return false;
        s.on_vkDestroyFence(device, fence, nullptr);
        handle_ended(as_number(fence));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetFences: {
        AV_SCOPE("server:vkResetFences");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t fenceCount{};
        VkFence* pFences_storage = nullptr;
        size_t pFences_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(fenceCount)) return false;
        pFences_storage_count = static_cast<size_t>(fenceCount);
        if (pFences_storage_count != 0) {
            pFences_storage = arena.allocate<VkFence>(pFences_storage_count);
            if (pFences_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pFences_storage_count; ++av_z) { pFences_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pFences_storage_count; ++av_i) {
            if (!deserialize_VkFence(r, pFences_storage[av_i])) return false;
        }
        VkResult result = s.on_vkResetFences(device, fenceCount, pFences_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitForFences: {
        AV_SCOPE("server:vkWaitForFences");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t fenceCount{};
        VkFence* pFences_storage = nullptr;
        size_t pFences_storage_count = 0;
        VkBool32 waitAll{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(fenceCount)) return false;
        pFences_storage_count = static_cast<size_t>(fenceCount);
        if (pFences_storage_count != 0) {
            pFences_storage = arena.allocate<VkFence>(pFences_storage_count);
            if (pFences_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pFences_storage_count; ++av_z) { pFences_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pFences_storage_count; ++av_i) {
            if (!deserialize_VkFence(r, pFences_storage[av_i])) return false;
        }
        if (!r.read_pod(waitAll)) return false;
        if (!r.read_pod(timeout)) return false;
        VkResult result = s.on_vkWaitForFences(device, fenceCount, pFences_storage, waitAll, timeout);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetFenceStatus: {
        AV_SCOPE("server:vkGetFenceStatus");
        VkDevice device = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFence(r, fence)) return false;
        if (fence == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetFenceStatus(device, fence);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateCommandPool: {
        AV_SCOPE("server:vkCreateCommandPool");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pCreateInfo_local{};
        VkCommandPool pCommandPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateCommandPool(device, &pCreateInfo_local, nullptr, &pCommandPool_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pCommandPool_out = VK_NULL_HANDLE;
        }
        serialize_VkCommandPool(reply, pCommandPool_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyCommandPool: {
        AV_SCOPE("server:vkDestroyCommandPool");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        s.on_vkDestroyCommandPool(device, commandPool, nullptr);
        handle_ended(as_number(commandPool));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetCommandPool: {
        AV_SCOPE("server:vkResetCommandPool");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolResetFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (commandPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(flags)) return false;
        VkResult result = s.on_vkResetCommandPool(device, commandPool, flags);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBeginCommandBuffer: {
        AV_SCOPE("server:vkBeginCommandBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferBeginInfo pBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCommandBufferBeginInfo(r, &pBeginInfo_local, arena)) return false;
        VkResult result = s.on_vkBeginCommandBuffer(commandBuffer, &pBeginInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkEndCommandBuffer: {
        AV_SCOPE("server:vkEndCommandBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        VkResult result = s.on_vkEndCommandBuffer(commandBuffer);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetCommandBuffer: {
        AV_SCOPE("server:vkResetCommandBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferResetFlags flags{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(flags)) return false;
        VkResult result = s.on_vkResetCommandBuffer(commandBuffer, flags);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyShaderModule: {
        AV_SCOPE("server:vkDestroyShaderModule");
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModule(r, shaderModule)) return false;
        s.on_vkDestroyShaderModule(device, shaderModule, nullptr);
        handle_ended(as_number(shaderModule));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateShaderModule: {
        AV_SCOPE("server:vkCreateShaderModule");
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo pCreateInfo_local{};
        VkShaderModule pShaderModule_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModuleCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateShaderModule(device, &pCreateInfo_local, nullptr, &pShaderModule_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pShaderModule_out = VK_NULL_HANDLE;
        }
        serialize_VkShaderModule(reply, pShaderModule_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineLayout: {
        AV_SCOPE("server:vkCreatePipelineLayout");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo pCreateInfo_local{};
        VkPipelineLayout pPipelineLayout_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreatePipelineLayout(device, &pCreateInfo_local, nullptr, &pPipelineLayout_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pPipelineLayout_out = VK_NULL_HANDLE;
        }
        serialize_VkPipelineLayout(reply, pPipelineLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineLayout: {
        AV_SCOPE("server:vkDestroyPipelineLayout");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineLayout(r, pipelineLayout)) return false;
        s.on_vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        handle_ended(as_number(pipelineLayout));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipeline: {
        AV_SCOPE("server:vkDestroyPipeline");
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        s.on_vkDestroyPipeline(device, pipeline, nullptr);
        handle_ended(as_number(pipeline));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineCache: {
        AV_SCOPE("server:vkCreatePipelineCache");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCacheCreateInfo pCreateInfo_local{};
        VkPipelineCache pPipelineCache_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCacheCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreatePipelineCache(device, &pCreateInfo_local, nullptr, &pPipelineCache_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pPipelineCache_out = VK_NULL_HANDLE;
        }
        serialize_VkPipelineCache(reply, pPipelineCache_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineCache: {
        AV_SCOPE("server:vkDestroyPipelineCache");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        s.on_vkDestroyPipelineCache(device, pipelineCache, nullptr);
        handle_ended(as_number(pipelineCache));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass: {
        AV_SCOPE("server:vkCreateRenderPass");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateRenderPass(device, &pCreateInfo_local, nullptr, &pRenderPass_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pRenderPass_out = VK_NULL_HANDLE;
        }
        serialize_VkRenderPass(reply, pRenderPass_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyRenderPass: {
        AV_SCOPE("server:vkDestroyRenderPass");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPass(r, renderPass)) return false;
        s.on_vkDestroyRenderPass(device, renderPass, nullptr);
        handle_ended(as_number(renderPass));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateFramebuffer: {
        AV_SCOPE("server:vkCreateFramebuffer");
        VkDevice device = VK_NULL_HANDLE;
        VkFramebufferCreateInfo pCreateInfo_local{};
        VkFramebuffer pFramebuffer_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFramebufferCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateFramebuffer(device, &pCreateInfo_local, nullptr, &pFramebuffer_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pFramebuffer_out = VK_NULL_HANDLE;
        }
        serialize_VkFramebuffer(reply, pFramebuffer_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyFramebuffer: {
        AV_SCOPE("server:vkDestroyFramebuffer");
        VkDevice device = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFramebuffer(r, framebuffer)) return false;
        s.on_vkDestroyFramebuffer(device, framebuffer, nullptr);
        handle_ended(as_number(framebuffer));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorSetLayout: {
        AV_SCOPE("server:vkCreateDescriptorSetLayout");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayout pSetLayout_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDescriptorSetLayout(device, &pCreateInfo_local, nullptr, &pSetLayout_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pSetLayout_out = VK_NULL_HANDLE;
        }
        serialize_VkDescriptorSetLayout(reply, pSetLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorSetLayout: {
        AV_SCOPE("server:vkDestroyDescriptorSetLayout");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayout(r, descriptorSetLayout)) return false;
        s.on_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        handle_ended(as_number(descriptorSetLayout));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorPool: {
        AV_SCOPE("server:vkCreateDescriptorPool");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPoolCreateInfo pCreateInfo_local{};
        VkDescriptorPool pDescriptorPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDescriptorPool(device, &pCreateInfo_local, nullptr, &pDescriptorPool_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pDescriptorPool_out = VK_NULL_HANDLE;
        }
        serialize_VkDescriptorPool(reply, pDescriptorPool_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorPool: {
        AV_SCOPE("server:vkDestroyDescriptorPool");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        s.on_vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        handle_ended(as_number(descriptorPool));
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeCommandBuffers: {
        AV_SCOPE("server:vkFreeCommandBuffers");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        uint32_t commandBufferCount{};
        VkCommandBuffer* pCommandBuffers_storage = nullptr;
        size_t pCommandBuffers_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (commandPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(commandBufferCount)) return false;
        pCommandBuffers_storage_count = static_cast<size_t>(commandBufferCount);
        if (pCommandBuffers_storage_count != 0) {
            pCommandBuffers_storage = arena.allocate<VkCommandBuffer>(pCommandBuffers_storage_count);
            if (pCommandBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCommandBuffers_storage_count; ++av_z) { pCommandBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCommandBuffers_storage_count; ++av_i) {
            if (!deserialize_VkCommandBuffer(r, pCommandBuffers_storage[av_i])) return false;
        }
        s.on_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers_storage);
        for (size_t av_e = 0; av_e < pCommandBuffers_storage_count; ++av_e) {
            handle_ended(as_number(pCommandBuffers_storage[av_e]));
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit: {
        AV_SCOPE("server:vkQueueSubmit");
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        VkSubmitInfo* pSubmits_storage = nullptr;
        size_t pSubmits_storage_count = 0;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage_count = static_cast<size_t>(submitCount);
        if (pSubmits_storage_count != 0) {
            pSubmits_storage = arena.allocate<VkSubmitInfo>(pSubmits_storage_count);
            if (pSubmits_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pSubmits_storage_count; ++av_z) { pSubmits_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pSubmits_storage_count; ++av_i) {
            if (!deserialize_VkSubmitInfo(r, &pSubmits_storage[av_i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        VkResult result = s.on_vkQueueSubmit(queue, submitCount, pSubmits_storage, fence);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindPipeline: {
        AV_SCOPE("server:vkCmdBindPipeline");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (pipeline == VK_NULL_HANDLE) return false;
        s.on_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewport: {
        AV_SCOPE("server:vkCmdSetViewport");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstViewport{};
        uint32_t viewportCount{};
        VkViewport* pViewports_storage = nullptr;
        size_t pViewports_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstViewport)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage_count = static_cast<size_t>(viewportCount);
        if (pViewports_storage_count != 0) {
            pViewports_storage = arena.allocate<VkViewport>(pViewports_storage_count);
            if (pViewports_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pViewports_storage_count; ++av_z) { pViewports_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pViewports_storage_count; ++av_i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissor: {
        AV_SCOPE("server:vkCmdSetScissor");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstScissor{};
        uint32_t scissorCount{};
        VkRect2D* pScissors_storage = nullptr;
        size_t pScissors_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstScissor)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage_count = static_cast<size_t>(scissorCount);
        if (pScissors_storage_count != 0) {
            pScissors_storage = arena.allocate<VkRect2D>(pScissors_storage_count);
            if (pScissors_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pScissors_storage_count; ++av_z) { pScissors_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pScissors_storage_count; ++av_i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers: {
        AV_SCOPE("server:vkCmdBindVertexBuffers");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        VkBuffer* pBuffers_storage = nullptr;
        size_t pBuffers_storage_count = 0;
        VkDeviceSize* pOffsets_storage = nullptr;
        size_t pOffsets_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage_count = static_cast<size_t>(bindingCount);
        if (pBuffers_storage_count != 0) {
            pBuffers_storage = arena.allocate<VkBuffer>(pBuffers_storage_count);
            if (pBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBuffers_storage_count; ++av_z) { pBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[av_i])) return false;
        }
        pOffsets_storage_count = static_cast<size_t>(bindingCount);
        if (pOffsets_storage_count != 0) {
            pOffsets_storage = arena.allocate<VkDeviceSize>(pOffsets_storage_count);
            if (pOffsets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pOffsets_storage_count; ++av_z) { pOffsets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pOffsets_storage_count; ++av_i) {
            if (!r.read_pod(pOffsets_storage[av_i])) return false;
        }
        s.on_vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers_storage, pOffsets_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindIndexBuffer: {
        AV_SCOPE("server:vkCmdBindIndexBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkIndexType indexType{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(indexType)) return false;
        s.on_vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindDescriptorSets: {
        AV_SCOPE("server:vkCmdBindDescriptorSets");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipelineLayout layout = VK_NULL_HANDLE;
        uint32_t firstSet{};
        uint32_t descriptorSetCount{};
        VkDescriptorSet* pDescriptorSets_storage = nullptr;
        size_t pDescriptorSets_storage_count = 0;
        uint32_t dynamicOffsetCount{};
        uint32_t* pDynamicOffsets_storage = nullptr;
        size_t pDynamicOffsets_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (layout == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstSet)) return false;
        if (!r.read_pod(descriptorSetCount)) return false;
        pDescriptorSets_storage_count = static_cast<size_t>(descriptorSetCount);
        if (pDescriptorSets_storage_count != 0) {
            pDescriptorSets_storage = arena.allocate<VkDescriptorSet>(pDescriptorSets_storage_count);
            if (pDescriptorSets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorSets_storage_count; ++av_z) { pDescriptorSets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDescriptorSets_storage_count; ++av_i) {
            if (!deserialize_VkDescriptorSet(r, pDescriptorSets_storage[av_i])) return false;
        }
        if (!r.read_pod(dynamicOffsetCount)) return false;
        pDynamicOffsets_storage_count = static_cast<size_t>(dynamicOffsetCount);
        if (pDynamicOffsets_storage_count != 0) {
            pDynamicOffsets_storage = arena.allocate<uint32_t>(pDynamicOffsets_storage_count);
            if (pDynamicOffsets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDynamicOffsets_storage_count; ++av_z) { pDynamicOffsets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDynamicOffsets_storage_count; ++av_i) {
            if (!r.read_pod(pDynamicOffsets_storage[av_i])) return false;
        }
        s.on_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets_storage, dynamicOffsetCount, pDynamicOffsets_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDraw: {
        AV_SCOPE("server:vkCmdDraw");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t vertexCount{};
        uint32_t instanceCount{};
        uint32_t firstVertex{};
        uint32_t firstInstance{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(vertexCount)) return false;
        if (!r.read_pod(instanceCount)) return false;
        if (!r.read_pod(firstVertex)) return false;
        if (!r.read_pod(firstInstance)) return false;
        s.on_vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexed: {
        AV_SCOPE("server:vkCmdDrawIndexed");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t indexCount{};
        uint32_t instanceCount{};
        uint32_t firstIndex{};
        int32_t vertexOffset{};
        uint32_t firstInstance{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(indexCount)) return false;
        if (!r.read_pod(instanceCount)) return false;
        if (!r.read_pod(firstIndex)) return false;
        if (!r.read_pod(vertexOffset)) return false;
        if (!r.read_pod(firstInstance)) return false;
        s.on_vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier: {
        AV_SCOPE("server:vkCmdPipelineBarrier");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags srcStageMask{};
        VkPipelineStageFlags dstStageMask{};
        VkDependencyFlags dependencyFlags{};
        uint32_t memoryBarrierCount{};
        VkMemoryBarrier* pMemoryBarriers_storage = nullptr;
        size_t pMemoryBarriers_storage_count = 0;
        uint32_t bufferMemoryBarrierCount{};
        VkBufferMemoryBarrier* pBufferMemoryBarriers_storage = nullptr;
        size_t pBufferMemoryBarriers_storage_count = 0;
        uint32_t imageMemoryBarrierCount{};
        VkImageMemoryBarrier* pImageMemoryBarriers_storage = nullptr;
        size_t pImageMemoryBarriers_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(srcStageMask)) return false;
        if (!r.read_pod(dstStageMask)) return false;
        if (!r.read_pod(dependencyFlags)) return false;
        if (!r.read_pod(memoryBarrierCount)) return false;
        pMemoryBarriers_storage_count = static_cast<size_t>(memoryBarrierCount);
        if (pMemoryBarriers_storage_count != 0) {
            pMemoryBarriers_storage = arena.allocate<VkMemoryBarrier>(pMemoryBarriers_storage_count);
            if (pMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pMemoryBarriers_storage_count; ++av_z) { pMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkMemoryBarrier(r, &pMemoryBarriers_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(bufferMemoryBarrierCount)) return false;
        pBufferMemoryBarriers_storage_count = static_cast<size_t>(bufferMemoryBarrierCount);
        if (pBufferMemoryBarriers_storage_count != 0) {
            pBufferMemoryBarriers_storage = arena.allocate<VkBufferMemoryBarrier>(pBufferMemoryBarriers_storage_count);
            if (pBufferMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBufferMemoryBarriers_storage_count; ++av_z) { pBufferMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBufferMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkBufferMemoryBarrier(r, &pBufferMemoryBarriers_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(imageMemoryBarrierCount)) return false;
        pImageMemoryBarriers_storage_count = static_cast<size_t>(imageMemoryBarrierCount);
        if (pImageMemoryBarriers_storage_count != 0) {
            pImageMemoryBarriers_storage = arena.allocate<VkImageMemoryBarrier>(pImageMemoryBarriers_storage_count);
            if (pImageMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pImageMemoryBarriers_storage_count; ++av_z) { pImageMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pImageMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkImageMemoryBarrier(r, &pImageMemoryBarriers_storage[av_i], arena)) return false;
        }
        s.on_vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers_storage, bufferMemoryBarrierCount, pBufferMemoryBarriers_storage, imageMemoryBarrierCount, pImageMemoryBarriers_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer: {
        AV_SCOPE("server:vkCmdCopyBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer srcBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        uint32_t regionCount{};
        VkBufferCopy* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, srcBuffer)) return false;
        if (srcBuffer == VK_NULL_HANDLE) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkBufferCopy>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkBufferCopy(r, &pRegions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass: {
        AV_SCOPE("server:vkCmdBeginRenderPass");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassContents contents{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!r.read_pod(contents)) return false;
        s.on_vkCmdBeginRenderPass(commandBuffer, &pRenderPassBegin_local, contents);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass: {
        AV_SCOPE("server:vkCmdEndRenderPass");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        s.on_vkCmdEndRenderPass(commandBuffer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass: {
        AV_SCOPE("server:vkCmdNextSubpass");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassContents contents{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(contents)) return false;
        s.on_vkCmdNextSubpass(commandBuffer, contents);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushConstants: {
        AV_SCOPE("server:vkCmdPushConstants");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkShaderStageFlags stageFlags{};
        uint32_t offset{};
        uint32_t size{};
        uint8_t* pValues_storage = nullptr;
        size_t pValues_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (layout == VK_NULL_HANDLE) return false;
        if (!r.read_pod(stageFlags)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(size)) return false;
        pValues_storage_count = static_cast<size_t>(size);
        if (pValues_storage_count != 0) {
            pValues_storage = arena.allocate<uint8_t>(pValues_storage_count);
            if (pValues_storage == nullptr) return false;
            if (!r.read(pValues_storage, pValues_storage_count)) return false;
        }
        s.on_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateGraphicsPipelines: {
        AV_SCOPE("server:vkCreateGraphicsPipelines");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        VkGraphicsPipelineCreateInfo* pCreateInfos_storage = nullptr;
        size_t pCreateInfos_storage_count = 0;
        VkPipeline* pPipelines_storage = nullptr;
        size_t pPipelines_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage_count = static_cast<size_t>(createInfoCount);
        if (pCreateInfos_storage_count != 0) {
            pCreateInfos_storage = arena.allocate<VkGraphicsPipelineCreateInfo>(pCreateInfos_storage_count);
            if (pCreateInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCreateInfos_storage_count; ++av_z) { pCreateInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCreateInfos_storage_count; ++av_i) {
            if (!deserialize_VkGraphicsPipelineCreateInfo(r, &pCreateInfos_storage[av_i], arena)) return false;
        }
        pPipelines_storage_count = static_cast<size_t>(createInfoCount);
        if (pPipelines_storage_count != 0) {
            pPipelines_storage = arena.allocate<VkPipeline>(pPipelines_storage_count);
            if (pPipelines_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPipelines_storage_count)) ? s.on_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos_storage, nullptr, pPipelines_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pPipelines_storage_count; ++av_k) {
            serialize_VkPipeline(reply, pPipelines_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkUpdateDescriptorSets: {
        AV_SCOPE("server:vkUpdateDescriptorSets");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t descriptorWriteCount{};
        VkWriteDescriptorSet* pDescriptorWrites_storage = nullptr;
        size_t pDescriptorWrites_storage_count = 0;
        uint32_t descriptorCopyCount{};
        VkCopyDescriptorSet* pDescriptorCopies_storage = nullptr;
        size_t pDescriptorCopies_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(descriptorWriteCount)) return false;
        pDescriptorWrites_storage_count = static_cast<size_t>(descriptorWriteCount);
        if (pDescriptorWrites_storage_count != 0) {
            pDescriptorWrites_storage = arena.allocate<VkWriteDescriptorSet>(pDescriptorWrites_storage_count);
            if (pDescriptorWrites_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorWrites_storage_count; ++av_z) { pDescriptorWrites_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDescriptorWrites_storage_count; ++av_i) {
            if (!deserialize_VkWriteDescriptorSet(r, &pDescriptorWrites_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(descriptorCopyCount)) return false;
        pDescriptorCopies_storage_count = static_cast<size_t>(descriptorCopyCount);
        if (pDescriptorCopies_storage_count != 0) {
            pDescriptorCopies_storage = arena.allocate<VkCopyDescriptorSet>(pDescriptorCopies_storage_count);
            if (pDescriptorCopies_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorCopies_storage_count; ++av_z) { pDescriptorCopies_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDescriptorCopies_storage_count; ++av_i) {
            if (!deserialize_VkCopyDescriptorSet(r, &pDescriptorCopies_storage[av_i], arena)) return false;
        }
        s.on_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites_storage, descriptorCopyCount, pDescriptorCopies_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateCommandBuffers: {
        AV_SCOPE("server:vkAllocateCommandBuffers");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo pAllocateInfo_local{};
        VkCommandBuffer* pCommandBuffers_storage = nullptr;
        size_t pCommandBuffers_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandBufferAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        pCommandBuffers_storage_count = static_cast<size_t>(pAllocateInfo_local.commandBufferCount);
        if (pCommandBuffers_storage_count != 0) {
            pCommandBuffers_storage = arena.allocate<VkCommandBuffer>(pCommandBuffers_storage_count);
            if (pCommandBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCommandBuffers_storage_count; ++av_z) { pCommandBuffers_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pCommandBuffers_storage_count)) ? s.on_vkAllocateCommandBuffers(device, &pAllocateInfo_local, pCommandBuffers_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(pAllocateInfo_local.commandPool));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pCommandBuffers_storage_count; ++av_z) { pCommandBuffers_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pCommandBuffers_storage_count; ++av_k) {
            serialize_VkCommandBuffer(reply, pCommandBuffers_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateDescriptorSets: {
        AV_SCOPE("server:vkAllocateDescriptorSets");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo pAllocateInfo_local{};
        VkDescriptorSet* pDescriptorSets_storage = nullptr;
        size_t pDescriptorSets_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        pDescriptorSets_storage_count = static_cast<size_t>(pAllocateInfo_local.descriptorSetCount);
        if (pDescriptorSets_storage_count != 0) {
            pDescriptorSets_storage = arena.allocate<VkDescriptorSet>(pDescriptorSets_storage_count);
            if (pDescriptorSets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorSets_storage_count; ++av_z) { pDescriptorSets_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pDescriptorSets_storage_count)) ? s.on_vkAllocateDescriptorSets(device, &pAllocateInfo_local, pDescriptorSets_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(pAllocateInfo_local.descriptorPool));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pDescriptorSets_storage_count; ++av_z) { pDescriptorSets_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pDescriptorSets_storage_count; ++av_k) {
            serialize_VkDescriptorSet(reply, pDescriptorSets_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeDescriptorSets: {
        AV_SCOPE("server:vkFreeDescriptorSets");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        uint32_t descriptorSetCount{};
        VkDescriptorSet* pDescriptorSets_storage = nullptr;
        size_t pDescriptorSets_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        if (descriptorPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(descriptorSetCount)) return false;
        pDescriptorSets_storage_count = static_cast<size_t>(descriptorSetCount);
        if (pDescriptorSets_storage_count != 0) {
            pDescriptorSets_storage = arena.allocate<VkDescriptorSet>(pDescriptorSets_storage_count);
            if (pDescriptorSets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorSets_storage_count; ++av_z) { pDescriptorSets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDescriptorSets_storage_count; ++av_i) {
            if (!deserialize_VkDescriptorSet(r, pDescriptorSets_storage[av_i])) return false;
        }
        VkResult result = s.on_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets_storage);
        for (size_t av_e = 0; av_e < pDescriptorSets_storage_count; ++av_e) {
            handle_ended(as_number(pDescriptorSets_storage[av_e]));
        }
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateComputePipelines: {
        AV_SCOPE("server:vkCreateComputePipelines");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        VkComputePipelineCreateInfo* pCreateInfos_storage = nullptr;
        size_t pCreateInfos_storage_count = 0;
        VkPipeline* pPipelines_storage = nullptr;
        size_t pPipelines_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage_count = static_cast<size_t>(createInfoCount);
        if (pCreateInfos_storage_count != 0) {
            pCreateInfos_storage = arena.allocate<VkComputePipelineCreateInfo>(pCreateInfos_storage_count);
            if (pCreateInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCreateInfos_storage_count; ++av_z) { pCreateInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCreateInfos_storage_count; ++av_i) {
            if (!deserialize_VkComputePipelineCreateInfo(r, &pCreateInfos_storage[av_i], arena)) return false;
        }
        pPipelines_storage_count = static_cast<size_t>(createInfoCount);
        if (pPipelines_storage_count != 0) {
            pPipelines_storage = arena.allocate<VkPipeline>(pPipelines_storage_count);
            if (pPipelines_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPipelines_storage_count)) ? s.on_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos_storage, nullptr, pPipelines_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pPipelines_storage_count; ++av_k) {
            serialize_VkPipeline(reply, pPipelines_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkResetDescriptorPool: {
        AV_SCOPE("server:vkResetDescriptorPool");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorPoolResetFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        if (descriptorPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(flags)) return false;
        VkResult result = s.on_vkResetDescriptorPool(device, descriptorPool, flags);
        handle_children_ended(as_number(descriptorPool));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateEvent: {
        AV_SCOPE("server:vkCreateEvent");
        VkDevice device = VK_NULL_HANDLE;
        VkEventCreateInfo pCreateInfo_local{};
        VkEvent pEvent_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEventCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateEvent(device, &pCreateInfo_local, nullptr, &pEvent_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pEvent_out = VK_NULL_HANDLE;
        }
        serialize_VkEvent(reply, pEvent_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyEvent: {
        AV_SCOPE("server:vkDestroyEvent");
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        s.on_vkDestroyEvent(device, event, nullptr);
        handle_ended(as_number(event));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetEventStatus: {
        AV_SCOPE("server:vkGetEventStatus");
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetEventStatus(device, event);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSetEvent: {
        AV_SCOPE("server:vkSetEvent");
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkSetEvent(device, event);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetEvent: {
        AV_SCOPE("server:vkResetEvent");
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkResetEvent(device, event);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent: {
        AV_SCOPE("server:vkCmdSetEvent");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!r.read_pod(stageMask)) return false;
        s.on_vkCmdSetEvent(commandBuffer, event, stageMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent: {
        AV_SCOPE("server:vkCmdResetEvent");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!r.read_pod(stageMask)) return false;
        s.on_vkCmdResetEvent(commandBuffer, event, stageMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents: {
        AV_SCOPE("server:vkCmdWaitEvents");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        VkEvent* pEvents_storage = nullptr;
        size_t pEvents_storage_count = 0;
        VkPipelineStageFlags srcStageMask{};
        VkPipelineStageFlags dstStageMask{};
        uint32_t memoryBarrierCount{};
        VkMemoryBarrier* pMemoryBarriers_storage = nullptr;
        size_t pMemoryBarriers_storage_count = 0;
        uint32_t bufferMemoryBarrierCount{};
        VkBufferMemoryBarrier* pBufferMemoryBarriers_storage = nullptr;
        size_t pBufferMemoryBarriers_storage_count = 0;
        uint32_t imageMemoryBarrierCount{};
        VkImageMemoryBarrier* pImageMemoryBarriers_storage = nullptr;
        size_t pImageMemoryBarriers_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage_count = static_cast<size_t>(eventCount);
        if (pEvents_storage_count != 0) {
            pEvents_storage = arena.allocate<VkEvent>(pEvents_storage_count);
            if (pEvents_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pEvents_storage_count; ++av_z) { pEvents_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pEvents_storage_count; ++av_i) {
            if (!deserialize_VkEvent(r, pEvents_storage[av_i])) return false;
        }
        if (!r.read_pod(srcStageMask)) return false;
        if (!r.read_pod(dstStageMask)) return false;
        if (!r.read_pod(memoryBarrierCount)) return false;
        pMemoryBarriers_storage_count = static_cast<size_t>(memoryBarrierCount);
        if (pMemoryBarriers_storage_count != 0) {
            pMemoryBarriers_storage = arena.allocate<VkMemoryBarrier>(pMemoryBarriers_storage_count);
            if (pMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pMemoryBarriers_storage_count; ++av_z) { pMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkMemoryBarrier(r, &pMemoryBarriers_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(bufferMemoryBarrierCount)) return false;
        pBufferMemoryBarriers_storage_count = static_cast<size_t>(bufferMemoryBarrierCount);
        if (pBufferMemoryBarriers_storage_count != 0) {
            pBufferMemoryBarriers_storage = arena.allocate<VkBufferMemoryBarrier>(pBufferMemoryBarriers_storage_count);
            if (pBufferMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBufferMemoryBarriers_storage_count; ++av_z) { pBufferMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBufferMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkBufferMemoryBarrier(r, &pBufferMemoryBarriers_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(imageMemoryBarrierCount)) return false;
        pImageMemoryBarriers_storage_count = static_cast<size_t>(imageMemoryBarrierCount);
        if (pImageMemoryBarriers_storage_count != 0) {
            pImageMemoryBarriers_storage = arena.allocate<VkImageMemoryBarrier>(pImageMemoryBarriers_storage_count);
            if (pImageMemoryBarriers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pImageMemoryBarriers_storage_count; ++av_z) { pImageMemoryBarriers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pImageMemoryBarriers_storage_count; ++av_i) {
            if (!deserialize_VkImageMemoryBarrier(r, &pImageMemoryBarriers_storage[av_i], arena)) return false;
        }
        s.on_vkCmdWaitEvents(commandBuffer, eventCount, pEvents_storage, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers_storage, bufferMemoryBarrierCount, pBufferMemoryBarriers_storage, imageMemoryBarrierCount, pImageMemoryBarriers_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage: {
        AV_SCOPE("server:vkCmdCopyImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        VkImageCopy* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (srcImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (dstImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkImageCopy>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkImageCopy(r, &pRegions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage: {
        AV_SCOPE("server:vkCmdCopyBufferToImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer srcBuffer = VK_NULL_HANDLE;
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        VkBufferImageCopy* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, srcBuffer)) return false;
        if (srcBuffer == VK_NULL_HANDLE) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (dstImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkBufferImageCopy>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkBufferImageCopy(r, &pRegions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer: {
        AV_SCOPE("server:vkCmdCopyImageToBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        uint32_t regionCount{};
        VkBufferImageCopy* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (srcImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkBufferImageCopy>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkBufferImageCopy(r, &pRegions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage: {
        AV_SCOPE("server:vkCmdBlitImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        VkImageBlit* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        VkFilter filter{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (srcImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (dstImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkImageBlit>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkImageBlit(r, &pRegions_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(filter)) return false;
        s.on_vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage, filter);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearColorImage: {
        AV_SCOPE("server:vkCmdClearColorImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout imageLayout{};
        VkClearColorValue pColor_local{};
        uint32_t rangeCount{};
        VkImageSubresourceRange* pRanges_storage = nullptr;
        size_t pRanges_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!r.read_pod(imageLayout)) return false;
        if (!deserialize_VkClearColorValue(r, &pColor_local, arena)) return false;
        if (!r.read_pod(rangeCount)) return false;
        pRanges_storage_count = static_cast<size_t>(rangeCount);
        if (pRanges_storage_count != 0) {
            pRanges_storage = arena.allocate<VkImageSubresourceRange>(pRanges_storage_count);
            if (pRanges_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRanges_storage_count; ++av_z) { pRanges_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRanges_storage_count; ++av_i) {
            if (!deserialize_VkImageSubresourceRange(r, &pRanges_storage[av_i], arena)) return false;
        }
        s.on_vkCmdClearColorImage(commandBuffer, image, imageLayout, &pColor_local, rangeCount, pRanges_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearDepthStencilImage: {
        AV_SCOPE("server:vkCmdClearDepthStencilImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout imageLayout{};
        VkClearDepthStencilValue pDepthStencil_local{};
        uint32_t rangeCount{};
        VkImageSubresourceRange* pRanges_storage = nullptr;
        size_t pRanges_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!r.read_pod(imageLayout)) return false;
        if (!deserialize_VkClearDepthStencilValue(r, &pDepthStencil_local, arena)) return false;
        if (!r.read_pod(rangeCount)) return false;
        pRanges_storage_count = static_cast<size_t>(rangeCount);
        if (pRanges_storage_count != 0) {
            pRanges_storage = arena.allocate<VkImageSubresourceRange>(pRanges_storage_count);
            if (pRanges_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRanges_storage_count; ++av_z) { pRanges_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRanges_storage_count; ++av_i) {
            if (!deserialize_VkImageSubresourceRange(r, &pRanges_storage[av_i], arena)) return false;
        }
        s.on_vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, &pDepthStencil_local, rangeCount, pRanges_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearAttachments: {
        AV_SCOPE("server:vkCmdClearAttachments");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t attachmentCount{};
        VkClearAttachment* pAttachments_storage = nullptr;
        size_t pAttachments_storage_count = 0;
        uint32_t rectCount{};
        VkClearRect* pRects_storage = nullptr;
        size_t pRects_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pAttachments_storage_count = static_cast<size_t>(attachmentCount);
        if (pAttachments_storage_count != 0) {
            pAttachments_storage = arena.allocate<VkClearAttachment>(pAttachments_storage_count);
            if (pAttachments_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pAttachments_storage_count; ++av_z) { pAttachments_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pAttachments_storage_count; ++av_i) {
            if (!deserialize_VkClearAttachment(r, &pAttachments_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(rectCount)) return false;
        pRects_storage_count = static_cast<size_t>(rectCount);
        if (pRects_storage_count != 0) {
            pRects_storage = arena.allocate<VkClearRect>(pRects_storage_count);
            if (pRects_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRects_storage_count; ++av_z) { pRects_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRects_storage_count; ++av_i) {
            if (!deserialize_VkClearRect(r, &pRects_storage[av_i], arena)) return false;
        }
        s.on_vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments_storage, rectCount, pRects_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdFillBuffer: {
        AV_SCOPE("server:vkCmdFillBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        VkDeviceSize size{};
        uint32_t data{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(size)) return false;
        if (!r.read_pod(data)) return false;
        s.on_vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdUpdateBuffer: {
        AV_SCOPE("server:vkCmdUpdateBuffer");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        VkDeviceSize dataSize{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(dataSize)) return false;
        pData_storage_count = static_cast<size_t>(dataSize);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            if (!r.read(pData_storage, pData_storage_count)) return false;
        }
        s.on_vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineWidth: {
        AV_SCOPE("server:vkCmdSetLineWidth");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float lineWidth{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineWidth)) return false;
        s.on_vkCmdSetLineWidth(commandBuffer, lineWidth);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBias: {
        AV_SCOPE("server:vkCmdSetDepthBias");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float depthBiasConstantFactor{};
        float depthBiasClamp{};
        float depthBiasSlopeFactor{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasConstantFactor)) return false;
        if (!r.read_pod(depthBiasClamp)) return false;
        if (!r.read_pod(depthBiasSlopeFactor)) return false;
        s.on_vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetBlendConstants: {
        AV_SCOPE("server:vkCmdSetBlendConstants");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float blendConstants_local[4]{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        for (uint32_t av_i = 0; av_i < 4u; ++av_i) {
            if (!r.read_pod(blendConstants_local[av_i])) return false;
        }
        s.on_vkCmdSetBlendConstants(commandBuffer, blendConstants_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBounds: {
        AV_SCOPE("server:vkCmdSetDepthBounds");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float minDepthBounds{};
        float maxDepthBounds{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(minDepthBounds)) return false;
        if (!r.read_pod(maxDepthBounds)) return false;
        s.on_vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilCompareMask: {
        AV_SCOPE("server:vkCmdSetStencilCompareMask");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t compareMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(compareMask)) return false;
        s.on_vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilWriteMask: {
        AV_SCOPE("server:vkCmdSetStencilWriteMask");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t writeMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(writeMask)) return false;
        s.on_vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilReference: {
        AV_SCOPE("server:vkCmdSetStencilReference");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t reference{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(reference)) return false;
        s.on_vkCmdSetStencilReference(commandBuffer, faceMask, reference);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatch: {
        AV_SCOPE("server:vkCmdDispatch");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        s.on_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchIndirect: {
        AV_SCOPE("server:vkCmdDispatchIndirect");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        s.on_vkCmdDispatchIndirect(commandBuffer, buffer, offset);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirect: {
        AV_SCOPE("server:vkCmdDrawIndirect");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirect: {
        AV_SCOPE("server:vkCmdDrawIndexedIndirect");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdExecuteCommands: {
        AV_SCOPE("server:vkCmdExecuteCommands");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t commandBufferCount{};
        VkCommandBuffer* pCommandBuffers_storage = nullptr;
        size_t pCommandBuffers_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(commandBufferCount)) return false;
        pCommandBuffers_storage_count = static_cast<size_t>(commandBufferCount);
        if (pCommandBuffers_storage_count != 0) {
            pCommandBuffers_storage = arena.allocate<VkCommandBuffer>(pCommandBuffers_storage_count);
            if (pCommandBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCommandBuffers_storage_count; ++av_z) { pCommandBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCommandBuffers_storage_count; ++av_i) {
            if (!deserialize_VkCommandBuffer(r, pCommandBuffers_storage[av_i])) return false;
        }
        s.on_vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkAcquireNextImage2KHR: {
        AV_SCOPE("server:vkAcquireNextImage2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAcquireNextImageInfoKHR pAcquireInfo_local{};
        uint32_t pImageIndex_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAcquireNextImageInfoKHR(r, &pAcquireInfo_local, arena)) return false;
        VkResult result = s.on_vkAcquireNextImage2KHR(device, &pAcquireInfo_local, &pImageIndex_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pImageIndex_out = {};
        }
        reply.write_pod(pImageIndex_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory2: {
        AV_SCOPE("server:vkBindBufferMemory2");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        VkBindBufferMemoryInfo* pBindInfos_storage = nullptr;
        size_t pBindInfos_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage_count = static_cast<size_t>(bindInfoCount);
        if (pBindInfos_storage_count != 0) {
            pBindInfos_storage = arena.allocate<VkBindBufferMemoryInfo>(pBindInfos_storage_count);
            if (pBindInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBindInfos_storage_count; ++av_z) { pBindInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBindInfos_storage_count; ++av_i) {
            if (!deserialize_VkBindBufferMemoryInfo(r, &pBindInfos_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkBindBufferMemory2(device, bindInfoCount, pBindInfos_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory2: {
        AV_SCOPE("server:vkBindImageMemory2");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        VkBindImageMemoryInfo* pBindInfos_storage = nullptr;
        size_t pBindInfos_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage_count = static_cast<size_t>(bindInfoCount);
        if (pBindInfos_storage_count != 0) {
            pBindInfos_storage = arena.allocate<VkBindImageMemoryInfo>(pBindInfos_storage_count);
            if (pBindInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBindInfos_storage_count; ++av_z) { pBindInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBindInfos_storage_count; ++av_i) {
            if (!deserialize_VkBindImageMemoryInfo(r, &pBindInfos_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkBindImageMemory2(device, bindInfoCount, pBindInfos_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginQuery: {
        AV_SCOPE("server:vkCmdBeginQuery");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        VkQueryControlFlags flags{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(flags)) return false;
        s.on_vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass2: {
        AV_SCOPE("server:vkCmdBeginRenderPass2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        s.on_vkCmdBeginRenderPass2(commandBuffer, &pRenderPassBegin_local, &pSubpassBeginInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRendering: {
        AV_SCOPE("server:vkCmdBeginRendering");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInfo pRenderingInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInfo(r, &pRenderingInfo_local, arena)) return false;
        s.on_vkCmdBeginRendering(commandBuffer, &pRenderingInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers2: {
        AV_SCOPE("server:vkCmdBindVertexBuffers2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        VkBuffer* pBuffers_storage = nullptr;
        size_t pBuffers_storage_count = 0;
        VkDeviceSize* pOffsets_storage = nullptr;
        size_t pOffsets_storage_count = 0;
        VkDeviceSize* pSizes_storage = nullptr;
        size_t pSizes_storage_count = 0;
        uint8_t pSizes_present = 0;
        VkDeviceSize* pStrides_storage = nullptr;
        size_t pStrides_storage_count = 0;
        uint8_t pStrides_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage_count = static_cast<size_t>(bindingCount);
        if (pBuffers_storage_count != 0) {
            pBuffers_storage = arena.allocate<VkBuffer>(pBuffers_storage_count);
            if (pBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBuffers_storage_count; ++av_z) { pBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[av_i])) return false;
        }
        pOffsets_storage_count = static_cast<size_t>(bindingCount);
        if (pOffsets_storage_count != 0) {
            pOffsets_storage = arena.allocate<VkDeviceSize>(pOffsets_storage_count);
            if (pOffsets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pOffsets_storage_count; ++av_z) { pOffsets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pOffsets_storage_count; ++av_i) {
            if (!r.read_pod(pOffsets_storage[av_i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage_count = static_cast<size_t>(bindingCount);
            if (pSizes_storage_count != 0) {
                pSizes_storage = arena.allocate<VkDeviceSize>(pSizes_storage_count);
                if (pSizes_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSizes_storage_count; ++av_z) { pSizes_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pSizes_storage_count; ++av_i) {
                if (!r.read_pod(pSizes_storage[av_i])) return false;
            }
        }
        if (!r.read_pod(pStrides_present)) return false;
        if (pStrides_present) {
            pStrides_storage_count = static_cast<size_t>(bindingCount);
            if (pStrides_storage_count != 0) {
                pStrides_storage = arena.allocate<VkDeviceSize>(pStrides_storage_count);
                if (pStrides_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pStrides_storage_count; ++av_z) { pStrides_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pStrides_storage_count; ++av_i) {
                if (!r.read_pod(pStrides_storage[av_i])) return false;
            }
        }
        s.on_vkCmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers_storage, pOffsets_storage, pSizes_present ? pSizes_storage : nullptr, pStrides_present ? pStrides_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage2: {
        AV_SCOPE("server:vkCmdBlitImage2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBlitImageInfo2 pBlitImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBlitImageInfo2(r, &pBlitImageInfo_local, arena)) return false;
        s.on_vkCmdBlitImage2(commandBuffer, &pBlitImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer2: {
        AV_SCOPE("server:vkCmdCopyBuffer2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferInfo2 pCopyBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferInfo2(r, &pCopyBufferInfo_local, arena)) return false;
        s.on_vkCmdCopyBuffer2(commandBuffer, &pCopyBufferInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage2: {
        AV_SCOPE("server:vkCmdCopyBufferToImage2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferToImageInfo2 pCopyBufferToImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferToImageInfo2(r, &pCopyBufferToImageInfo_local, arena)) return false;
        s.on_vkCmdCopyBufferToImage2(commandBuffer, &pCopyBufferToImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage2: {
        AV_SCOPE("server:vkCmdCopyImage2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageInfo2 pCopyImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageInfo2(r, &pCopyImageInfo_local, arena)) return false;
        s.on_vkCmdCopyImage2(commandBuffer, &pCopyImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer2: {
        AV_SCOPE("server:vkCmdCopyImageToBuffer2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageToBufferInfo2 pCopyImageToBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageToBufferInfo2(r, &pCopyImageToBufferInfo_local, arena)) return false;
        s.on_vkCmdCopyImageToBuffer2(commandBuffer, &pCopyImageToBufferInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyQueryPoolResults: {
        AV_SCOPE("server:vkCmdCopyQueryPoolResults");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        VkDeviceSize stride{};
        VkQueryResultFlags flags{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(stride)) return false;
        if (!r.read_pod(flags)) return false;
        s.on_vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchBase: {
        AV_SCOPE("server:vkCmdDispatchBase");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t baseGroupX{};
        uint32_t baseGroupY{};
        uint32_t baseGroupZ{};
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(baseGroupX)) return false;
        if (!r.read_pod(baseGroupY)) return false;
        if (!r.read_pod(baseGroupZ)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        s.on_vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount: {
        AV_SCOPE("server:vkCmdDrawIndexedIndirectCount");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (countBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectCount: {
        AV_SCOPE("server:vkCmdDrawIndirectCount");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (countBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndQuery: {
        AV_SCOPE("server:vkCmdEndQuery");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        s.on_vkCmdEndQuery(commandBuffer, queryPool, query);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass2: {
        AV_SCOPE("server:vkCmdEndRenderPass2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        s.on_vkCmdEndRenderPass2(commandBuffer, &pSubpassEndInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRendering: {
        AV_SCOPE("server:vkCmdEndRendering");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        s.on_vkCmdEndRendering(commandBuffer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass2: {
        AV_SCOPE("server:vkCmdNextSubpass2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        s.on_vkCmdNextSubpass2(commandBuffer, &pSubpassBeginInfo_local, &pSubpassEndInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier2: {
        AV_SCOPE("server:vkCmdPipelineBarrier2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        s.on_vkCmdPipelineBarrier2(commandBuffer, &pDependencyInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent2: {
        AV_SCOPE("server:vkCmdResetEvent2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!r.read_pod(stageMask)) return false;
        s.on_vkCmdResetEvent2(commandBuffer, event, stageMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetQueryPool: {
        AV_SCOPE("server:vkCmdResetQueryPool");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        s.on_vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage: {
        AV_SCOPE("server:vkCmdResolveImage");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        VkImageResolve* pRegions_storage = nullptr;
        size_t pRegions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (srcImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (dstImage == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage_count = static_cast<size_t>(regionCount);
        if (pRegions_storage_count != 0) {
            pRegions_storage = arena.allocate<VkImageResolve>(pRegions_storage_count);
            if (pRegions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pRegions_storage_count; ++av_z) { pRegions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pRegions_storage_count; ++av_i) {
            if (!deserialize_VkImageResolve(r, &pRegions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage2: {
        AV_SCOPE("server:vkCmdResolveImage2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResolveImageInfo2 pResolveImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkResolveImageInfo2(r, &pResolveImageInfo_local, arena)) return false;
        s.on_vkCmdResolveImage2(commandBuffer, &pResolveImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCullMode: {
        AV_SCOPE("server:vkCmdSetCullMode");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCullModeFlags cullMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(cullMode)) return false;
        s.on_vkCmdSetCullMode(commandBuffer, cullMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBiasEnable: {
        AV_SCOPE("server:vkCmdSetDepthBiasEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBiasEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasEnable)) return false;
        s.on_vkCmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable: {
        AV_SCOPE("server:vkCmdSetDepthBoundsTestEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBoundsTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBoundsTestEnable)) return false;
        s.on_vkCmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthCompareOp: {
        AV_SCOPE("server:vkCmdSetDepthCompareOp");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCompareOp depthCompareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthCompareOp)) return false;
        s.on_vkCmdSetDepthCompareOp(commandBuffer, depthCompareOp);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthTestEnable: {
        AV_SCOPE("server:vkCmdSetDepthTestEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthTestEnable)) return false;
        s.on_vkCmdSetDepthTestEnable(commandBuffer, depthTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthWriteEnable: {
        AV_SCOPE("server:vkCmdSetDepthWriteEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthWriteEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthWriteEnable)) return false;
        s.on_vkCmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDeviceMask: {
        AV_SCOPE("server:vkCmdSetDeviceMask");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t deviceMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(deviceMask)) return false;
        s.on_vkCmdSetDeviceMask(commandBuffer, deviceMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent2: {
        AV_SCOPE("server:vkCmdSetEvent2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        s.on_vkCmdSetEvent2(commandBuffer, event, &pDependencyInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFrontFace: {
        AV_SCOPE("server:vkCmdSetFrontFace");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFrontFace frontFace{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(frontFace)) return false;
        s.on_vkCmdSetFrontFace(commandBuffer, frontFace);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable: {
        AV_SCOPE("server:vkCmdSetPrimitiveRestartEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 primitiveRestartEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveRestartEnable)) return false;
        s.on_vkCmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveTopology: {
        AV_SCOPE("server:vkCmdSetPrimitiveTopology");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPrimitiveTopology primitiveTopology{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveTopology)) return false;
        s.on_vkCmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable: {
        AV_SCOPE("server:vkCmdSetRasterizerDiscardEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 rasterizerDiscardEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizerDiscardEnable)) return false;
        s.on_vkCmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissorWithCount: {
        AV_SCOPE("server:vkCmdSetScissorWithCount");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t scissorCount{};
        VkRect2D* pScissors_storage = nullptr;
        size_t pScissors_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage_count = static_cast<size_t>(scissorCount);
        if (pScissors_storage_count != 0) {
            pScissors_storage = arena.allocate<VkRect2D>(pScissors_storage_count);
            if (pScissors_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pScissors_storage_count; ++av_z) { pScissors_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pScissors_storage_count; ++av_i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetScissorWithCount(commandBuffer, scissorCount, pScissors_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilOp: {
        AV_SCOPE("server:vkCmdSetStencilOp");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        VkStencilOp failOp{};
        VkStencilOp passOp{};
        VkStencilOp depthFailOp{};
        VkCompareOp compareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(failOp)) return false;
        if (!r.read_pod(passOp)) return false;
        if (!r.read_pod(depthFailOp)) return false;
        if (!r.read_pod(compareOp)) return false;
        s.on_vkCmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilTestEnable: {
        AV_SCOPE("server:vkCmdSetStencilTestEnable");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stencilTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stencilTestEnable)) return false;
        s.on_vkCmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWithCount: {
        AV_SCOPE("server:vkCmdSetViewportWithCount");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t viewportCount{};
        VkViewport* pViewports_storage = nullptr;
        size_t pViewports_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage_count = static_cast<size_t>(viewportCount);
        if (pViewports_storage_count != 0) {
            pViewports_storage = arena.allocate<VkViewport>(pViewports_storage_count);
            if (pViewports_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pViewports_storage_count; ++av_z) { pViewports_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pViewports_storage_count; ++av_i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetViewportWithCount(commandBuffer, viewportCount, pViewports_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents2: {
        AV_SCOPE("server:vkCmdWaitEvents2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        VkEvent* pEvents_storage = nullptr;
        size_t pEvents_storage_count = 0;
        VkDependencyInfo* pDependencyInfos_storage = nullptr;
        size_t pDependencyInfos_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage_count = static_cast<size_t>(eventCount);
        if (pEvents_storage_count != 0) {
            pEvents_storage = arena.allocate<VkEvent>(pEvents_storage_count);
            if (pEvents_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pEvents_storage_count; ++av_z) { pEvents_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pEvents_storage_count; ++av_i) {
            if (!deserialize_VkEvent(r, pEvents_storage[av_i])) return false;
        }
        pDependencyInfos_storage_count = static_cast<size_t>(eventCount);
        if (pDependencyInfos_storage_count != 0) {
            pDependencyInfos_storage = arena.allocate<VkDependencyInfo>(pDependencyInfos_storage_count);
            if (pDependencyInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDependencyInfos_storage_count; ++av_z) { pDependencyInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDependencyInfos_storage_count; ++av_i) {
            if (!deserialize_VkDependencyInfo(r, &pDependencyInfos_storage[av_i], arena)) return false;
        }
        s.on_vkCmdWaitEvents2(commandBuffer, eventCount, pEvents_storage, pDependencyInfos_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp: {
        AV_SCOPE("server:vkCmdWriteTimestamp");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlagBits pipelineStage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineStage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        s.on_vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp2: {
        AV_SCOPE("server:vkCmdWriteTimestamp2");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        s.on_vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateBufferView: {
        AV_SCOPE("server:vkCreateBufferView");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferViewCreateInfo pCreateInfo_local{};
        VkBufferView pView_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferViewCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateBufferView(device, &pCreateInfo_local, nullptr, &pView_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pView_out = VK_NULL_HANDLE;
        }
        serialize_VkBufferView(reply, pView_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplate: {
        AV_SCOPE("server:vkCreateDescriptorUpdateTemplate");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplateCreateInfo pCreateInfo_local{};
        VkDescriptorUpdateTemplate pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplateCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDescriptorUpdateTemplate(device, &pCreateInfo_local, nullptr, &pDescriptorUpdateTemplate_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        }
        serialize_VkDescriptorUpdateTemplate(reply, pDescriptorUpdateTemplate_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePrivateDataSlot: {
        AV_SCOPE("server:vkCreatePrivateDataSlot");
        VkDevice device = VK_NULL_HANDLE;
        VkPrivateDataSlotCreateInfo pCreateInfo_local{};
        VkPrivateDataSlot pPrivateDataSlot_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPrivateDataSlotCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreatePrivateDataSlot(device, &pCreateInfo_local, nullptr, &pPrivateDataSlot_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pPrivateDataSlot_out = VK_NULL_HANDLE;
        }
        serialize_VkPrivateDataSlot(reply, pPrivateDataSlot_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateQueryPool: {
        AV_SCOPE("server:vkCreateQueryPool");
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPoolCreateInfo pCreateInfo_local{};
        VkQueryPool pQueryPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateQueryPool(device, &pCreateInfo_local, nullptr, &pQueryPool_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pQueryPool_out = VK_NULL_HANDLE;
        }
        serialize_VkQueryPool(reply, pQueryPool_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass2: {
        AV_SCOPE("server:vkCreateRenderPass2");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo2 pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo2(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateRenderPass2(device, &pCreateInfo_local, nullptr, &pRenderPass_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pRenderPass_out = VK_NULL_HANDLE;
        }
        serialize_VkRenderPass(reply, pRenderPass_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSamplerYcbcrConversion: {
        AV_SCOPE("server:vkCreateSamplerYcbcrConversion");
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversionCreateInfo pCreateInfo_local{};
        VkSamplerYcbcrConversion pYcbcrConversion_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversionCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateSamplerYcbcrConversion(device, &pCreateInfo_local, nullptr, &pYcbcrConversion_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pYcbcrConversion_out = VK_NULL_HANDLE;
        }
        serialize_VkSamplerYcbcrConversion(reply, pYcbcrConversion_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyBufferView: {
        AV_SCOPE("server:vkDestroyBufferView");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferView bufferView = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferView(r, bufferView)) return false;
        s.on_vkDestroyBufferView(device, bufferView, nullptr);
        handle_ended(as_number(bufferView));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate: {
        AV_SCOPE("server:vkDestroyDescriptorUpdateTemplate");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        s.on_vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
        handle_ended(as_number(descriptorUpdateTemplate));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPrivateDataSlot: {
        AV_SCOPE("server:vkDestroyPrivateDataSlot");
        VkDevice device = VK_NULL_HANDLE;
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        s.on_vkDestroyPrivateDataSlot(device, privateDataSlot, nullptr);
        handle_ended(as_number(privateDataSlot));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyQueryPool: {
        AV_SCOPE("server:vkDestroyQueryPool");
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        s.on_vkDestroyQueryPool(device, queryPool, nullptr);
        handle_ended(as_number(queryPool));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion: {
        AV_SCOPE("server:vkDestroySamplerYcbcrConversion");
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversion(r, ycbcrConversion)) return false;
        s.on_vkDestroySamplerYcbcrConversion(device, ycbcrConversion, nullptr);
        handle_ended(as_number(ycbcrConversion));
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroups: {
        AV_SCOPE("server:vkEnumeratePhysicalDeviceGroups");
        VkInstance instance = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pPhysicalDeviceGroupCount_local = 0;
        VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties_storage = nullptr;
        size_t pPhysicalDeviceGroupProperties_storage_count = 0;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceGroupCount_local)) return false;
            pPhysicalDeviceGroupProperties_storage_count = static_cast<size_t>(pPhysicalDeviceGroupCount_local);
            if (pPhysicalDeviceGroupProperties_storage_count != 0) {
                pPhysicalDeviceGroupProperties_storage = arena.allocate<VkPhysicalDeviceGroupProperties>(pPhysicalDeviceGroupProperties_storage_count);
                if (pPhysicalDeviceGroupProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pPhysicalDeviceGroupProperties_storage_count; ++av_z) { pPhysicalDeviceGroupProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pPhysicalDeviceGroupProperties_storage_count; ++av_e) { pPhysicalDeviceGroupProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES; pPhysicalDeviceGroupProperties_storage[av_e].pNext = nullptr; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPhysicalDeviceGroupProperties_storage_count * 32u)) ? s.on_vkEnumeratePhysicalDeviceGroups(instance, &pPhysicalDeviceGroupCount_local, array_present ? pPhysicalDeviceGroupProperties_storage : nullptr) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(instance));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPhysicalDeviceGroupCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPhysicalDeviceGroupCount_local;
        if (array_present && pPhysicalDeviceGroupCount_sent > pPhysicalDeviceGroupProperties_storage_count) {
            pPhysicalDeviceGroupCount_sent = static_cast<uint32_t>(pPhysicalDeviceGroupProperties_storage_count);
        }
        reply.write_pod(pPhysicalDeviceGroupCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPhysicalDeviceGroupCount_sent; ++k) {
                serialize_VkPhysicalDeviceGroupProperties(reply, pPhysicalDeviceGroupProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkFlushMappedMemoryRanges: {
        AV_SCOPE("server:vkFlushMappedMemoryRanges");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t memoryRangeCount{};
        VkMappedMemoryRange* pMemoryRanges_storage = nullptr;
        size_t pMemoryRanges_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(memoryRangeCount)) return false;
        pMemoryRanges_storage_count = static_cast<size_t>(memoryRangeCount);
        if (pMemoryRanges_storage_count != 0) {
            pMemoryRanges_storage = arena.allocate<VkMappedMemoryRange>(pMemoryRanges_storage_count);
            if (pMemoryRanges_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pMemoryRanges_storage_count; ++av_z) { pMemoryRanges_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pMemoryRanges_storage_count; ++av_i) {
            if (!deserialize_VkMappedMemoryRange(r, &pMemoryRanges_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferDeviceAddress: {
        AV_SCOPE("server:vkGetBufferDeviceAddress");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        VkDeviceAddress result = s.on_vkGetBufferDeviceAddress(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements2: {
        AV_SCOPE("server:vkGetBufferMemoryRequirements2");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetBufferMemoryRequirements2(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddress: {
        AV_SCOPE("server:vkGetBufferOpaqueCaptureAddress");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        uint64_t result = s.on_vkGetBufferOpaqueCaptureAddress(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport: {
        AV_SCOPE("server:vkGetDescriptorSetLayoutSupport");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayoutSupport pSupport_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pSupport_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSupport_out), arena)) return false;
        s.on_vkGetDescriptorSetLayoutSupport(device, &pCreateInfo_local, &pSupport_out);
        reply.write_pod<int32_t>(0);
        serialize_VkDescriptorSetLayoutSupport(reply, pSupport_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements: {
        AV_SCOPE("server:vkGetDeviceBufferMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceBufferMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceBufferMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetDeviceBufferMemoryRequirements(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeatures: {
        AV_SCOPE("server:vkGetDeviceGroupPeerMemoryFeatures");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t heapIndex{};
        uint32_t localDeviceIndex{};
        uint32_t remoteDeviceIndex{};
        VkPeerMemoryFeatureFlags pPeerMemoryFeatures_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(heapIndex)) return false;
        if (!r.read_pod(localDeviceIndex)) return false;
        if (!r.read_pod(remoteDeviceIndex)) return false;
        s.on_vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, &pPeerMemoryFeatures_out);
        reply.write_pod<int32_t>(0);
        reply.write_pod(pPeerMemoryFeatures_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR: {
        AV_SCOPE("server:vkGetDeviceGroupPresentCapabilitiesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceGroupPresentCapabilitiesKHR pDeviceGroupPresentCapabilities_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(pDeviceGroupPresentCapabilities_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pDeviceGroupPresentCapabilities_out), arena)) return false;
        VkResult result = s.on_vkGetDeviceGroupPresentCapabilitiesKHR(device, &pDeviceGroupPresentCapabilities_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkDeviceGroupPresentCapabilitiesKHR pDeviceGroupPresentCapabilities_blank{};
            pDeviceGroupPresentCapabilities_blank.sType = pDeviceGroupPresentCapabilities_out.sType;
            pDeviceGroupPresentCapabilities_out = pDeviceGroupPresentCapabilities_blank;
        }
        serialize_VkDeviceGroupPresentCapabilitiesKHR(reply, pDeviceGroupPresentCapabilities_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupSurfacePresentModesKHR: {
        AV_SCOPE("server:vkGetDeviceGroupSurfacePresentModesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkDeviceGroupPresentModeFlagsKHR pModes_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSurfaceKHR(r, surface)) return false;
        if (surface == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetDeviceGroupSurfacePresentModesKHR(device, surface, &pModes_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pModes_out = {};
        }
        reply.write_pod(pModes_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements: {
        AV_SCOPE("server:vkGetDeviceImageMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetDeviceImageMemoryRequirements(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirements: {
        AV_SCOPE("server:vkGetDeviceImageSparseMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        uint8_t array_present = 0;
        uint32_t pSparseMemoryRequirementCount_local = 0;
        VkSparseImageMemoryRequirements2* pSparseMemoryRequirements_storage = nullptr;
        size_t pSparseMemoryRequirements_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage_count = static_cast<size_t>(pSparseMemoryRequirementCount_local);
            if (pSparseMemoryRequirements_storage_count != 0) {
                pSparseMemoryRequirements_storage = arena.allocate<VkSparseImageMemoryRequirements2>(pSparseMemoryRequirements_storage_count);
                if (pSparseMemoryRequirements_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSparseMemoryRequirements_storage_count; ++av_z) { pSparseMemoryRequirements_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pSparseMemoryRequirements_storage_count; ++av_e) { pSparseMemoryRequirements_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; pSparseMemoryRequirements_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetDeviceImageSparseMemoryRequirements(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pSparseMemoryRequirementCount_sent = pSparseMemoryRequirementCount_local;
        if (array_present && pSparseMemoryRequirementCount_sent > pSparseMemoryRequirements_storage_count) {
            pSparseMemoryRequirementCount_sent = static_cast<uint32_t>(pSparseMemoryRequirements_storage_count);
        }
        reply.write_pod(pSparseMemoryRequirementCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pSparseMemoryRequirementCount_sent; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryCommitment: {
        AV_SCOPE("server:vkGetDeviceMemoryCommitment");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize pCommittedMemoryInBytes_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (memory == VK_NULL_HANDLE) return false;
        s.on_vkGetDeviceMemoryCommitment(device, memory, &pCommittedMemoryInBytes_out);
        reply.write_pod<int32_t>(0);
        reply.write_pod(pCommittedMemoryInBytes_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress: {
        AV_SCOPE("server:vkGetDeviceMemoryOpaqueCaptureAddress");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemoryOpaqueCaptureAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemoryOpaqueCaptureAddressInfo(r, &pInfo_local, arena)) return false;
        uint64_t result = s.on_vkGetDeviceMemoryOpaqueCaptureAddress(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceQueue2: {
        AV_SCOPE("server:vkGetDeviceQueue2");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceQueueInfo2 pQueueInfo_local{};
        VkQueue pQueue_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceQueueInfo2(r, &pQueueInfo_local, arena)) return false;
        if (handles_room(static_cast<uint64_t>(1u))) s.on_vkGetDeviceQueue2(device, &pQueueInfo_local, &pQueue_out);
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(0);
        serialize_VkQueue(reply, pQueue_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements2: {
        AV_SCOPE("server:vkGetImageMemoryRequirements2");
        VkDevice device = VK_NULL_HANDLE;
        VkImageMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetImageMemoryRequirements2(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements: {
        AV_SCOPE("server:vkGetImageSparseMemoryRequirements");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pSparseMemoryRequirementCount_local = 0;
        VkSparseImageMemoryRequirements* pSparseMemoryRequirements_storage = nullptr;
        size_t pSparseMemoryRequirements_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage_count = static_cast<size_t>(pSparseMemoryRequirementCount_local);
            if (pSparseMemoryRequirements_storage_count != 0) {
                pSparseMemoryRequirements_storage = arena.allocate<VkSparseImageMemoryRequirements>(pSparseMemoryRequirements_storage_count);
                if (pSparseMemoryRequirements_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSparseMemoryRequirements_storage_count; ++av_z) { pSparseMemoryRequirements_storage[av_z] = {}; }
            }
        }
        s.on_vkGetImageSparseMemoryRequirements(device, image, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pSparseMemoryRequirementCount_sent = pSparseMemoryRequirementCount_local;
        if (array_present && pSparseMemoryRequirementCount_sent > pSparseMemoryRequirements_storage_count) {
            pSparseMemoryRequirementCount_sent = static_cast<uint32_t>(pSparseMemoryRequirements_storage_count);
        }
        reply.write_pod(pSparseMemoryRequirementCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pSparseMemoryRequirementCount_sent; ++k) {
                serialize_VkSparseImageMemoryRequirements(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2: {
        AV_SCOPE("server:vkGetImageSparseMemoryRequirements2");
        VkDevice device = VK_NULL_HANDLE;
        VkImageSparseMemoryRequirementsInfo2 pInfo_local{};
        uint8_t array_present = 0;
        uint32_t pSparseMemoryRequirementCount_local = 0;
        VkSparseImageMemoryRequirements2* pSparseMemoryRequirements_storage = nullptr;
        size_t pSparseMemoryRequirements_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageSparseMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage_count = static_cast<size_t>(pSparseMemoryRequirementCount_local);
            if (pSparseMemoryRequirements_storage_count != 0) {
                pSparseMemoryRequirements_storage = arena.allocate<VkSparseImageMemoryRequirements2>(pSparseMemoryRequirements_storage_count);
                if (pSparseMemoryRequirements_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSparseMemoryRequirements_storage_count; ++av_z) { pSparseMemoryRequirements_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pSparseMemoryRequirements_storage_count; ++av_e) { pSparseMemoryRequirements_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; pSparseMemoryRequirements_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetImageSparseMemoryRequirements2(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pSparseMemoryRequirementCount_sent = pSparseMemoryRequirementCount_local;
        if (array_present && pSparseMemoryRequirementCount_sent > pSparseMemoryRequirements_storage_count) {
            pSparseMemoryRequirementCount_sent = static_cast<uint32_t>(pSparseMemoryRequirements_storage_count);
        }
        reply.write_pod(pSparseMemoryRequirementCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pSparseMemoryRequirementCount_sent; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout: {
        AV_SCOPE("server:vkGetImageSubresourceLayout");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource pSubresource_local{};
        VkSubresourceLayout pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!deserialize_VkImageSubresource(r, &pSubresource_local, arena)) return false;
        s.on_vkGetImageSubresourceLayout(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<int32_t>(0);
        serialize_VkSubresourceLayout(reply, pLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalBufferProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalBufferInfo pExternalBufferInfo_local{};
        VkExternalBufferProperties pExternalBufferProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalBufferInfo(r, &pExternalBufferInfo_local, arena)) return false;
        if (!r.read_pod(pExternalBufferProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalBufferProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, &pExternalBufferInfo_local, &pExternalBufferProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalBufferProperties(reply, pExternalBufferProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalFenceProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalFenceInfo pExternalFenceInfo_local{};
        VkExternalFenceProperties pExternalFenceProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalFenceInfo(r, &pExternalFenceInfo_local, arena)) return false;
        if (!r.read_pod(pExternalFenceProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalFenceProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, &pExternalFenceInfo_local, &pExternalFenceProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalFenceProperties(reply, pExternalFenceProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalSemaphoreProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalSemaphoreInfo pExternalSemaphoreInfo_local{};
        VkExternalSemaphoreProperties pExternalSemaphoreProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalSemaphoreInfo(r, &pExternalSemaphoreInfo_local, arena)) return false;
        if (!r.read_pod(pExternalSemaphoreProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalSemaphoreProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, &pExternalSemaphoreInfo_local, &pExternalSemaphoreProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalSemaphoreProperties(reply, pExternalSemaphoreProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDevicePresentRectanglesKHR: {
        AV_SCOPE("server:vkGetPhysicalDevicePresentRectanglesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pRectCount_local = 0;
        VkRect2D* pRects_storage = nullptr;
        size_t pRects_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkSurfaceKHR(r, surface)) return false;
        if (surface == VK_NULL_HANDLE) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pRectCount_local)) return false;
            pRects_storage_count = static_cast<size_t>(pRectCount_local);
            if (pRects_storage_count != 0) {
                pRects_storage = arena.allocate<VkRect2D>(pRects_storage_count);
                if (pRects_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pRects_storage_count; ++av_z) { pRects_storage[av_z] = {}; }
            }
        }
        VkResult result = s.on_vkGetPhysicalDevicePresentRectanglesKHR(physicalDevice, surface, &pRectCount_local, array_present ? pRects_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pRectCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pRectCount_local;
        if (array_present && pRectCount_sent > pRects_storage_count) {
            pRectCount_sent = static_cast<uint32_t>(pRects_storage_count);
        }
        reply.write_pod(pRectCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pRectCount_sent; ++k) {
                serialize_VkRect2D(reply, pRects_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceToolProperties: {
        AV_SCOPE("server:vkGetPhysicalDeviceToolProperties");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pToolCount_local = 0;
        VkPhysicalDeviceToolProperties* pToolProperties_storage = nullptr;
        size_t pToolProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pToolCount_local)) return false;
            pToolProperties_storage_count = static_cast<size_t>(pToolCount_local);
            if (pToolProperties_storage_count != 0) {
                pToolProperties_storage = arena.allocate<VkPhysicalDeviceToolProperties>(pToolProperties_storage_count);
                if (pToolProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pToolProperties_storage_count; ++av_z) { pToolProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pToolProperties_storage_count; ++av_e) { pToolProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES; pToolProperties_storage[av_e].pNext = nullptr; }
        }
        VkResult result = s.on_vkGetPhysicalDeviceToolProperties(physicalDevice, &pToolCount_local, array_present ? pToolProperties_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pToolCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pToolCount_local;
        if (array_present && pToolCount_sent > pToolProperties_storage_count) {
            pToolCount_sent = static_cast<uint32_t>(pToolProperties_storage_count);
        }
        reply.write_pod(pToolCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pToolCount_sent; ++k) {
                serialize_VkPhysicalDeviceToolProperties(reply, pToolProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPrivateData: {
        AV_SCOPE("server:vkGetPrivateData");
        VkDevice device = VK_NULL_HANDLE;
        VkObjectType objectType{};
        uint64_t objectHandle{};
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        uint64_t pData_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(objectType)) return false;
        if (!r.read_pod(objectHandle)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        if (privateDataSlot == VK_NULL_HANDLE) return false;
        s.on_vkGetPrivateData(device, objectType, objectHandle, privateDataSlot, &pData_out);
        reply.write_pod<int32_t>(0);
        reply.write_pod(pData_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetQueryPoolResults: {
        AV_SCOPE("server:vkGetQueryPoolResults");
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        size_t dataSize{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        VkDeviceSize stride{};
        VkQueryResultFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        {
            uint64_t av_wide = 0;
            if (!r.read_pod(av_wide)) return false;
            dataSize = static_cast<size_t>(av_wide);
        }
        pData_storage_count = static_cast<size_t>(dataSize);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        if (!r.read_pod(stride)) return false;
        if (!r.read_pod(flags)) return false;
        VkResult result = s.on_vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData_storage, stride, flags);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        if (pData_storage_count != 0) {
            reply.write(pData_storage, pData_storage_count);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRenderAreaGranularity: {
        AV_SCOPE("server:vkGetRenderAreaGranularity");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkExtent2D pGranularity_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPass(r, renderPass)) return false;
        if (renderPass == VK_NULL_HANDLE) return false;
        s.on_vkGetRenderAreaGranularity(device, renderPass, &pGranularity_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExtent2D(reply, pGranularity_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreCounterValue: {
        AV_SCOPE("server:vkGetSemaphoreCounterValue");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        uint64_t pValue_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        if (semaphore == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetSemaphoreCounterValue(device, semaphore, &pValue_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pValue_out = {};
        }
        reply.write_pod(pValue_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkInvalidateMappedMemoryRanges: {
        AV_SCOPE("server:vkInvalidateMappedMemoryRanges");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t memoryRangeCount{};
        VkMappedMemoryRange* pMemoryRanges_storage = nullptr;
        size_t pMemoryRanges_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(memoryRangeCount)) return false;
        pMemoryRanges_storage_count = static_cast<size_t>(memoryRangeCount);
        if (pMemoryRanges_storage_count != 0) {
            pMemoryRanges_storage = arena.allocate<VkMappedMemoryRange>(pMemoryRanges_storage_count);
            if (pMemoryRanges_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pMemoryRanges_storage_count; ++av_z) { pMemoryRanges_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pMemoryRanges_storage_count; ++av_i) {
            if (!deserialize_VkMappedMemoryRange(r, &pMemoryRanges_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkMergePipelineCaches: {
        AV_SCOPE("server:vkMergePipelineCaches");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache dstCache = VK_NULL_HANDLE;
        uint32_t srcCacheCount{};
        VkPipelineCache* pSrcCaches_storage = nullptr;
        size_t pSrcCaches_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, dstCache)) return false;
        if (dstCache == VK_NULL_HANDLE) return false;
        if (!r.read_pod(srcCacheCount)) return false;
        pSrcCaches_storage_count = static_cast<size_t>(srcCacheCount);
        if (pSrcCaches_storage_count != 0) {
            pSrcCaches_storage = arena.allocate<VkPipelineCache>(pSrcCaches_storage_count);
            if (pSrcCaches_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pSrcCaches_storage_count; ++av_z) { pSrcCaches_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pSrcCaches_storage_count; ++av_i) {
            if (!deserialize_VkPipelineCache(r, pSrcCaches_storage[av_i])) return false;
        }
        VkResult result = s.on_vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueBindSparse: {
        AV_SCOPE("server:vkQueueBindSparse");
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        VkBindSparseInfo* pBindInfo_storage = nullptr;
        size_t pBindInfo_storage_count = 0;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfo_storage_count = static_cast<size_t>(bindInfoCount);
        if (pBindInfo_storage_count != 0) {
            pBindInfo_storage = arena.allocate<VkBindSparseInfo>(pBindInfo_storage_count);
            if (pBindInfo_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBindInfo_storage_count; ++av_z) { pBindInfo_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBindInfo_storage_count; ++av_i) {
            if (!deserialize_VkBindSparseInfo(r, &pBindInfo_storage[av_i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        VkResult result = s.on_vkQueueBindSparse(queue, bindInfoCount, pBindInfo_storage, fence);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit2: {
        AV_SCOPE("server:vkQueueSubmit2");
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        VkSubmitInfo2* pSubmits_storage = nullptr;
        size_t pSubmits_storage_count = 0;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage_count = static_cast<size_t>(submitCount);
        if (pSubmits_storage_count != 0) {
            pSubmits_storage = arena.allocate<VkSubmitInfo2>(pSubmits_storage_count);
            if (pSubmits_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pSubmits_storage_count; ++av_z) { pSubmits_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pSubmits_storage_count; ++av_i) {
            if (!deserialize_VkSubmitInfo2(r, &pSubmits_storage[av_i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        VkResult result = s.on_vkQueueSubmit2(queue, submitCount, pSubmits_storage, fence);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetQueryPool: {
        AV_SCOPE("server:vkResetQueryPool");
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        s.on_vkResetQueryPool(device, queryPool, firstQuery, queryCount);
        return true;
    }
    case ::av::vkproto::kCmd_vkSetPrivateData: {
        AV_SCOPE("server:vkSetPrivateData");
        VkDevice device = VK_NULL_HANDLE;
        VkObjectType objectType{};
        uint64_t objectHandle{};
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        uint64_t data{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(objectType)) return false;
        if (!r.read_pod(objectHandle)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        if (privateDataSlot == VK_NULL_HANDLE) return false;
        if (!r.read_pod(data)) return false;
        VkResult result = s.on_vkSetPrivateData(device, objectType, objectHandle, privateDataSlot, data);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSignalSemaphore: {
        AV_SCOPE("server:vkSignalSemaphore");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreSignalInfo pSignalInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreSignalInfo(r, &pSignalInfo_local, arena)) return false;
        VkResult result = s.on_vkSignalSemaphore(device, &pSignalInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTrimCommandPool: {
        AV_SCOPE("server:vkTrimCommandPool");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolTrimFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (commandPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(flags)) return false;
        s.on_vkTrimCommandPool(device, commandPool, flags);
        return true;
    }
    case ::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplate: {
        AV_SCOPE("server:vkUpdateDescriptorSetWithTemplate");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        uint64_t av_blob = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSet(r, descriptorSet)) return false;
        if (descriptorSet == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        if (descriptorUpdateTemplate == VK_NULL_HANDLE) return false;
        if (!r.read_varint(av_blob)) return false;
        if (av_blob != descriptor_template_bytes(descriptorUpdateTemplate)) return false;
        pData_storage_count = static_cast<size_t>(av_blob);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            if (!r.read(pData_storage, pData_storage_count)) return false;
        }
        s.on_vkUpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitSemaphores: {
        AV_SCOPE("server:vkWaitSemaphores");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreWaitInfo pWaitInfo_local{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreWaitInfo(r, &pWaitInfo_local, arena)) return false;
        if (!r.read_pod(timeout)) return false;
        VkResult result = s.on_vkWaitSemaphores(device, &pWaitInfo_local, timeout);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceFeatures2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures2 pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pFeatures_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFeatures_out), arena)) return false;
        s.on_vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &pFeatures_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceFeatures2(reply, pFeatures_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceFormatProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties2 pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(pFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFormatProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceFormatProperties2KHR(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkFormatProperties2(reply, pFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceImageFormatProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceImageFormatInfo2 pImageFormatInfo_local{};
        VkImageFormatProperties2 pImageFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceImageFormatInfo2(r, &pImageFormatInfo_local, arena)) return false;
        if (!r.read_pod(pImageFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pImageFormatProperties_out), arena)) return false;
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties2KHR(physicalDevice, &pImageFormatInfo_local, &pImageFormatProperties_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkImageFormatProperties2 pImageFormatProperties_blank{};
            pImageFormatProperties_blank.sType = pImageFormatProperties_out.sType;
            pImageFormatProperties_out = pImageFormatProperties_blank;
        }
        serialize_VkImageFormatProperties2(reply, pImageFormatProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceMemoryProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties2 pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pMemoryProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceMemoryProperties2KHR(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties2(reply, pMemoryProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceProperties2KHR(physicalDevice, &pProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkPhysicalDeviceProperties2(reply, pProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceQueueFamilyProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pQueueFamilyPropertyCount_local = 0;
        VkQueueFamilyProperties2* pQueueFamilyProperties_storage = nullptr;
        size_t pQueueFamilyProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage_count = static_cast<size_t>(pQueueFamilyPropertyCount_local);
            if (pQueueFamilyProperties_storage_count != 0) {
                pQueueFamilyProperties_storage = arena.allocate<VkQueueFamilyProperties2>(pQueueFamilyProperties_storage_count);
                if (pQueueFamilyProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pQueueFamilyProperties_storage_count; ++av_z) { pQueueFamilyProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pQueueFamilyProperties_storage_count; ++av_e) { pQueueFamilyProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; pQueueFamilyProperties_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pQueueFamilyPropertyCount_sent = pQueueFamilyPropertyCount_local;
        if (array_present && pQueueFamilyPropertyCount_sent > pQueueFamilyProperties_storage_count) {
            pQueueFamilyPropertyCount_sent = static_cast<uint32_t>(pQueueFamilyProperties_storage_count);
        }
        reply.write_pod(pQueueFamilyPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pQueueFamilyPropertyCount_sent; ++k) {
                serialize_VkQueueFamilyProperties2(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2KHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceSparseImageFormatInfo2 pFormatInfo_local{};
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkSparseImageFormatProperties2* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceSparseImageFormatInfo2(r, &pFormatInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkSparseImageFormatProperties2>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pProperties_storage_count; ++av_e) { pProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2; pProperties_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice, &pFormatInfo_local, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pPropertyCount_sent = pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkSparseImageFormatProperties2(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory2KHR: {
        AV_SCOPE("server:vkBindBufferMemory2KHR");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        VkBindBufferMemoryInfo* pBindInfos_storage = nullptr;
        size_t pBindInfos_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage_count = static_cast<size_t>(bindInfoCount);
        if (pBindInfos_storage_count != 0) {
            pBindInfos_storage = arena.allocate<VkBindBufferMemoryInfo>(pBindInfos_storage_count);
            if (pBindInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBindInfos_storage_count; ++av_z) { pBindInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBindInfos_storage_count; ++av_i) {
            if (!deserialize_VkBindBufferMemoryInfo(r, &pBindInfos_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkBindBufferMemory2KHR(device, bindInfoCount, pBindInfos_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory2KHR: {
        AV_SCOPE("server:vkBindImageMemory2KHR");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        VkBindImageMemoryInfo* pBindInfos_storage = nullptr;
        size_t pBindInfos_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage_count = static_cast<size_t>(bindInfoCount);
        if (pBindInfos_storage_count != 0) {
            pBindInfos_storage = arena.allocate<VkBindImageMemoryInfo>(pBindInfos_storage_count);
            if (pBindInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBindInfos_storage_count; ++av_z) { pBindInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBindInfos_storage_count; ++av_i) {
            if (!deserialize_VkBindImageMemoryInfo(r, &pBindInfos_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkBindImageMemory2KHR(device, bindInfoCount, pBindInfos_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR: {
        AV_SCOPE("server:vkGetBufferMemoryRequirements2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetBufferMemoryRequirements2KHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR: {
        AV_SCOPE("server:vkGetImageMemoryRequirements2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkImageMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetImageMemoryRequirements2KHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2KHR: {
        AV_SCOPE("server:vkGetImageSparseMemoryRequirements2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkImageSparseMemoryRequirementsInfo2 pInfo_local{};
        uint8_t array_present = 0;
        uint32_t pSparseMemoryRequirementCount_local = 0;
        VkSparseImageMemoryRequirements2* pSparseMemoryRequirements_storage = nullptr;
        size_t pSparseMemoryRequirements_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageSparseMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage_count = static_cast<size_t>(pSparseMemoryRequirementCount_local);
            if (pSparseMemoryRequirements_storage_count != 0) {
                pSparseMemoryRequirements_storage = arena.allocate<VkSparseImageMemoryRequirements2>(pSparseMemoryRequirements_storage_count);
                if (pSparseMemoryRequirements_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSparseMemoryRequirements_storage_count; ++av_z) { pSparseMemoryRequirements_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pSparseMemoryRequirements_storage_count; ++av_e) { pSparseMemoryRequirements_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; pSparseMemoryRequirements_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetImageSparseMemoryRequirements2KHR(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pSparseMemoryRequirementCount_sent = pSparseMemoryRequirementCount_local;
        if (array_present && pSparseMemoryRequirementCount_sent > pSparseMemoryRequirements_storage_count) {
            pSparseMemoryRequirementCount_sent = static_cast<uint32_t>(pSparseMemoryRequirements_storage_count);
        }
        reply.write_pod(pSparseMemoryRequirementCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pSparseMemoryRequirementCount_sent; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR: {
        AV_SCOPE("server:vkCmdBeginRenderPass2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        s.on_vkCmdBeginRenderPass2KHR(commandBuffer, &pRenderPassBegin_local, &pSubpassBeginInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderingKHR: {
        AV_SCOPE("server:vkCmdBeginRenderingKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInfo pRenderingInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInfo(r, &pRenderingInfo_local, arena)) return false;
        s.on_vkCmdBeginRenderingKHR(commandBuffer, &pRenderingInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage2KHR: {
        AV_SCOPE("server:vkCmdBlitImage2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBlitImageInfo2 pBlitImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBlitImageInfo2(r, &pBlitImageInfo_local, arena)) return false;
        s.on_vkCmdBlitImage2KHR(commandBuffer, &pBlitImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer2KHR: {
        AV_SCOPE("server:vkCmdCopyBuffer2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferInfo2 pCopyBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferInfo2(r, &pCopyBufferInfo_local, arena)) return false;
        s.on_vkCmdCopyBuffer2KHR(commandBuffer, &pCopyBufferInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR: {
        AV_SCOPE("server:vkCmdCopyBufferToImage2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferToImageInfo2 pCopyBufferToImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferToImageInfo2(r, &pCopyBufferToImageInfo_local, arena)) return false;
        s.on_vkCmdCopyBufferToImage2KHR(commandBuffer, &pCopyBufferToImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage2KHR: {
        AV_SCOPE("server:vkCmdCopyImage2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageInfo2 pCopyImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageInfo2(r, &pCopyImageInfo_local, arena)) return false;
        s.on_vkCmdCopyImage2KHR(commandBuffer, &pCopyImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR: {
        AV_SCOPE("server:vkCmdCopyImageToBuffer2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageToBufferInfo2 pCopyImageToBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageToBufferInfo2(r, &pCopyImageToBufferInfo_local, arena)) return false;
        s.on_vkCmdCopyImageToBuffer2KHR(commandBuffer, &pCopyImageToBufferInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchBaseKHR: {
        AV_SCOPE("server:vkCmdDispatchBaseKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t baseGroupX{};
        uint32_t baseGroupY{};
        uint32_t baseGroupZ{};
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(baseGroupX)) return false;
        if (!r.read_pod(baseGroupY)) return false;
        if (!r.read_pod(baseGroupZ)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        s.on_vkCmdDispatchBaseKHR(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR: {
        AV_SCOPE("server:vkCmdDrawIndexedIndirectCountKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (countBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndexedIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR: {
        AV_SCOPE("server:vkCmdDrawIndirectCountKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (countBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass2KHR: {
        AV_SCOPE("server:vkCmdEndRenderPass2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        s.on_vkCmdEndRenderPass2KHR(commandBuffer, &pSubpassEndInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderingKHR: {
        AV_SCOPE("server:vkCmdEndRenderingKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        s.on_vkCmdEndRenderingKHR(commandBuffer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass2KHR: {
        AV_SCOPE("server:vkCmdNextSubpass2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        s.on_vkCmdNextSubpass2KHR(commandBuffer, &pSubpassBeginInfo_local, &pSubpassEndInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR: {
        AV_SCOPE("server:vkCmdPipelineBarrier2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        s.on_vkCmdPipelineBarrier2KHR(commandBuffer, &pDependencyInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushDescriptorSetWithTemplateKHR: {
        AV_SCOPE("server:vkCmdPushDescriptorSetWithTemplateKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        uint32_t set{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        uint64_t av_blob = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        if (descriptorUpdateTemplate == VK_NULL_HANDLE) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (layout == VK_NULL_HANDLE) return false;
        if (!r.read_pod(set)) return false;
        if (!r.read_varint(av_blob)) return false;
        if (av_blob != descriptor_template_bytes(descriptorUpdateTemplate)) return false;
        pData_storage_count = static_cast<size_t>(av_blob);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            if (!r.read(pData_storage, pData_storage_count)) return false;
        }
        s.on_vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, descriptorUpdateTemplate, layout, set, pData_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent2KHR: {
        AV_SCOPE("server:vkCmdResetEvent2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!r.read_pod(stageMask)) return false;
        s.on_vkCmdResetEvent2KHR(commandBuffer, event, stageMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage2KHR: {
        AV_SCOPE("server:vkCmdResolveImage2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResolveImageInfo2 pResolveImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkResolveImageInfo2(r, &pResolveImageInfo_local, arena)) return false;
        s.on_vkCmdResolveImage2KHR(commandBuffer, &pResolveImageInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR: {
        AV_SCOPE("server:vkCmdSetDeviceMaskKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t deviceMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(deviceMask)) return false;
        s.on_vkCmdSetDeviceMaskKHR(commandBuffer, deviceMask);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent2KHR: {
        AV_SCOPE("server:vkCmdSetEvent2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (event == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        s.on_vkCmdSetEvent2KHR(commandBuffer, event, &pDependencyInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents2KHR: {
        AV_SCOPE("server:vkCmdWaitEvents2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        VkEvent* pEvents_storage = nullptr;
        size_t pEvents_storage_count = 0;
        VkDependencyInfo* pDependencyInfos_storage = nullptr;
        size_t pDependencyInfos_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage_count = static_cast<size_t>(eventCount);
        if (pEvents_storage_count != 0) {
            pEvents_storage = arena.allocate<VkEvent>(pEvents_storage_count);
            if (pEvents_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pEvents_storage_count; ++av_z) { pEvents_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pEvents_storage_count; ++av_i) {
            if (!deserialize_VkEvent(r, pEvents_storage[av_i])) return false;
        }
        pDependencyInfos_storage_count = static_cast<size_t>(eventCount);
        if (pDependencyInfos_storage_count != 0) {
            pDependencyInfos_storage = arena.allocate<VkDependencyInfo>(pDependencyInfos_storage_count);
            if (pDependencyInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDependencyInfos_storage_count; ++av_z) { pDependencyInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDependencyInfos_storage_count; ++av_i) {
            if (!deserialize_VkDependencyInfo(r, &pDependencyInfos_storage[av_i], arena)) return false;
        }
        s.on_vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents_storage, pDependencyInfos_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR: {
        AV_SCOPE("server:vkCmdWriteTimestamp2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        s.on_vkCmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplateKHR: {
        AV_SCOPE("server:vkCreateDescriptorUpdateTemplateKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplateCreateInfo pCreateInfo_local{};
        VkDescriptorUpdateTemplate pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplateCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDescriptorUpdateTemplateKHR(device, &pCreateInfo_local, nullptr, &pDescriptorUpdateTemplate_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        }
        serialize_VkDescriptorUpdateTemplate(reply, pDescriptorUpdateTemplate_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass2KHR: {
        AV_SCOPE("server:vkCreateRenderPass2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo2 pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo2(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateRenderPass2KHR(device, &pCreateInfo_local, nullptr, &pRenderPass_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pRenderPass_out = VK_NULL_HANDLE;
        }
        serialize_VkRenderPass(reply, pRenderPass_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSamplerYcbcrConversionKHR: {
        AV_SCOPE("server:vkCreateSamplerYcbcrConversionKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversionCreateInfo pCreateInfo_local{};
        VkSamplerYcbcrConversion pYcbcrConversion_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversionCreateInfo(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateSamplerYcbcrConversionKHR(device, &pCreateInfo_local, nullptr, &pYcbcrConversion_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pYcbcrConversion_out = VK_NULL_HANDLE;
        }
        serialize_VkSamplerYcbcrConversion(reply, pYcbcrConversion_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR: {
        AV_SCOPE("server:vkDestroyDescriptorUpdateTemplateKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        s.on_vkDestroyDescriptorUpdateTemplateKHR(device, descriptorUpdateTemplate, nullptr);
        handle_ended(as_number(descriptorUpdateTemplate));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR: {
        AV_SCOPE("server:vkDestroySamplerYcbcrConversionKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversion(r, ycbcrConversion)) return false;
        s.on_vkDestroySamplerYcbcrConversionKHR(device, ycbcrConversion, nullptr);
        handle_ended(as_number(ycbcrConversion));
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroupsKHR: {
        AV_SCOPE("server:vkEnumeratePhysicalDeviceGroupsKHR");
        VkInstance instance = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pPhysicalDeviceGroupCount_local = 0;
        VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties_storage = nullptr;
        size_t pPhysicalDeviceGroupProperties_storage_count = 0;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceGroupCount_local)) return false;
            pPhysicalDeviceGroupProperties_storage_count = static_cast<size_t>(pPhysicalDeviceGroupCount_local);
            if (pPhysicalDeviceGroupProperties_storage_count != 0) {
                pPhysicalDeviceGroupProperties_storage = arena.allocate<VkPhysicalDeviceGroupProperties>(pPhysicalDeviceGroupProperties_storage_count);
                if (pPhysicalDeviceGroupProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pPhysicalDeviceGroupProperties_storage_count; ++av_z) { pPhysicalDeviceGroupProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pPhysicalDeviceGroupProperties_storage_count; ++av_e) { pPhysicalDeviceGroupProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES; pPhysicalDeviceGroupProperties_storage[av_e].pNext = nullptr; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPhysicalDeviceGroupProperties_storage_count * 32u)) ? s.on_vkEnumeratePhysicalDeviceGroupsKHR(instance, &pPhysicalDeviceGroupCount_local, array_present ? pPhysicalDeviceGroupProperties_storage : nullptr) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(instance));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPhysicalDeviceGroupCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPhysicalDeviceGroupCount_local;
        if (array_present && pPhysicalDeviceGroupCount_sent > pPhysicalDeviceGroupProperties_storage_count) {
            pPhysicalDeviceGroupCount_sent = static_cast<uint32_t>(pPhysicalDeviceGroupProperties_storage_count);
        }
        reply.write_pod(pPhysicalDeviceGroupCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPhysicalDeviceGroupCount_sent; ++k) {
                serialize_VkPhysicalDeviceGroupProperties(reply, pPhysicalDeviceGroupProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferDeviceAddressKHR: {
        AV_SCOPE("server:vkGetBufferDeviceAddressKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        VkDeviceAddress result = s.on_vkGetBufferDeviceAddressKHR(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR: {
        AV_SCOPE("server:vkGetBufferOpaqueCaptureAddressKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        uint64_t result = s.on_vkGetBufferOpaqueCaptureAddressKHR(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR: {
        AV_SCOPE("server:vkGetDescriptorSetLayoutSupportKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayoutSupport pSupport_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pSupport_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSupport_out), arena)) return false;
        s.on_vkGetDescriptorSetLayoutSupportKHR(device, &pCreateInfo_local, &pSupport_out);
        reply.write_pod<int32_t>(0);
        serialize_VkDescriptorSetLayoutSupport(reply, pSupport_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR: {
        AV_SCOPE("server:vkGetDeviceBufferMemoryRequirementsKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceBufferMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceBufferMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetDeviceBufferMemoryRequirementsKHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeaturesKHR: {
        AV_SCOPE("server:vkGetDeviceGroupPeerMemoryFeaturesKHR");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t heapIndex{};
        uint32_t localDeviceIndex{};
        uint32_t remoteDeviceIndex{};
        VkPeerMemoryFeatureFlags pPeerMemoryFeatures_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(heapIndex)) return false;
        if (!r.read_pod(localDeviceIndex)) return false;
        if (!r.read_pod(remoteDeviceIndex)) return false;
        s.on_vkGetDeviceGroupPeerMemoryFeaturesKHR(device, heapIndex, localDeviceIndex, remoteDeviceIndex, &pPeerMemoryFeatures_out);
        reply.write_pod<int32_t>(0);
        reply.write_pod(pPeerMemoryFeatures_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR: {
        AV_SCOPE("server:vkGetDeviceImageMemoryRequirementsKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        s.on_vkGetDeviceImageMemoryRequirementsKHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirementsKHR: {
        AV_SCOPE("server:vkGetDeviceImageSparseMemoryRequirementsKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        uint8_t array_present = 0;
        uint32_t pSparseMemoryRequirementCount_local = 0;
        VkSparseImageMemoryRequirements2* pSparseMemoryRequirements_storage = nullptr;
        size_t pSparseMemoryRequirements_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage_count = static_cast<size_t>(pSparseMemoryRequirementCount_local);
            if (pSparseMemoryRequirements_storage_count != 0) {
                pSparseMemoryRequirements_storage = arena.allocate<VkSparseImageMemoryRequirements2>(pSparseMemoryRequirements_storage_count);
                if (pSparseMemoryRequirements_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSparseMemoryRequirements_storage_count; ++av_z) { pSparseMemoryRequirements_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pSparseMemoryRequirements_storage_count; ++av_e) { pSparseMemoryRequirements_storage[av_e].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; pSparseMemoryRequirements_storage[av_e].pNext = nullptr; }
        }
        s.on_vkGetDeviceImageSparseMemoryRequirementsKHR(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage : nullptr);
        reply.write_pod<int32_t>(0);
        uint32_t pSparseMemoryRequirementCount_sent = pSparseMemoryRequirementCount_local;
        if (array_present && pSparseMemoryRequirementCount_sent > pSparseMemoryRequirements_storage_count) {
            pSparseMemoryRequirementCount_sent = static_cast<uint32_t>(pSparseMemoryRequirements_storage_count);
        }
        reply.write_pod(pSparseMemoryRequirementCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pSparseMemoryRequirementCount_sent; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR: {
        AV_SCOPE("server:vkGetDeviceMemoryOpaqueCaptureAddressKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemoryOpaqueCaptureAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemoryOpaqueCaptureAddressInfo(r, &pInfo_local, arena)) return false;
        uint64_t result = s.on_vkGetDeviceMemoryOpaqueCaptureAddressKHR(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalBufferPropertiesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalBufferInfo pExternalBufferInfo_local{};
        VkExternalBufferProperties pExternalBufferProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalBufferInfo(r, &pExternalBufferInfo_local, arena)) return false;
        if (!r.read_pod(pExternalBufferProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalBufferProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice, &pExternalBufferInfo_local, &pExternalBufferProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalBufferProperties(reply, pExternalBufferProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalFencePropertiesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalFenceInfo pExternalFenceInfo_local{};
        VkExternalFenceProperties pExternalFenceProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalFenceInfo(r, &pExternalFenceInfo_local, arena)) return false;
        if (!r.read_pod(pExternalFenceProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalFenceProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalFencePropertiesKHR(physicalDevice, &pExternalFenceInfo_local, &pExternalFenceProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalFenceProperties(reply, pExternalFenceProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalSemaphoreInfo pExternalSemaphoreInfo_local{};
        VkExternalSemaphoreProperties pExternalSemaphoreProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalSemaphoreInfo(r, &pExternalSemaphoreInfo_local, arena)) return false;
        if (!r.read_pod(pExternalSemaphoreProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalSemaphoreProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice, &pExternalSemaphoreInfo_local, &pExternalSemaphoreProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExternalSemaphoreProperties(reply, pExternalSemaphoreProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreCounterValueKHR: {
        AV_SCOPE("server:vkGetSemaphoreCounterValueKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        uint64_t pValue_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        if (semaphore == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetSemaphoreCounterValueKHR(device, semaphore, &pValue_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pValue_out = {};
        }
        reply.write_pod(pValue_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit2KHR: {
        AV_SCOPE("server:vkQueueSubmit2KHR");
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        VkSubmitInfo2* pSubmits_storage = nullptr;
        size_t pSubmits_storage_count = 0;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage_count = static_cast<size_t>(submitCount);
        if (pSubmits_storage_count != 0) {
            pSubmits_storage = arena.allocate<VkSubmitInfo2>(pSubmits_storage_count);
            if (pSubmits_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pSubmits_storage_count; ++av_z) { pSubmits_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pSubmits_storage_count; ++av_i) {
            if (!deserialize_VkSubmitInfo2(r, &pSubmits_storage[av_i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        VkResult result = s.on_vkQueueSubmit2KHR(queue, submitCount, pSubmits_storage, fence);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSignalSemaphoreKHR: {
        AV_SCOPE("server:vkSignalSemaphoreKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreSignalInfo pSignalInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreSignalInfo(r, &pSignalInfo_local, arena)) return false;
        VkResult result = s.on_vkSignalSemaphoreKHR(device, &pSignalInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTrimCommandPoolKHR: {
        AV_SCOPE("server:vkTrimCommandPoolKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolTrimFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (commandPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(flags)) return false;
        s.on_vkTrimCommandPoolKHR(device, commandPool, flags);
        return true;
    }
    case ::av::vkproto::kCmd_vkUpdateDescriptorSetWithTemplateKHR: {
        AV_SCOPE("server:vkUpdateDescriptorSetWithTemplateKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        uint64_t av_blob = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSet(r, descriptorSet)) return false;
        if (descriptorSet == VK_NULL_HANDLE) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        if (descriptorUpdateTemplate == VK_NULL_HANDLE) return false;
        if (!r.read_varint(av_blob)) return false;
        if (av_blob != descriptor_template_bytes(descriptorUpdateTemplate)) return false;
        pData_storage_count = static_cast<size_t>(av_blob);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            if (!r.read(pData_storage, pData_storage_count)) return false;
        }
        s.on_vkUpdateDescriptorSetWithTemplateKHR(device, descriptorSet, descriptorUpdateTemplate, pData_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitSemaphoresKHR: {
        AV_SCOPE("server:vkWaitSemaphoresKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreWaitInfo pWaitInfo_local{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreWaitInfo(r, &pWaitInfo_local, arena)) return false;
        if (!r.read_pod(timeout)) return false;
        VkResult result = s.on_vkWaitSemaphoresKHR(device, &pWaitInfo_local, timeout);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT: {
        AV_SCOPE("server:vkCmdBeginConditionalRenderingEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkConditionalRenderingBeginInfoEXT pConditionalRenderingBegin_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkConditionalRenderingBeginInfoEXT(r, &pConditionalRenderingBegin_local, arena)) return false;
        s.on_vkCmdBeginConditionalRenderingEXT(commandBuffer, &pConditionalRenderingBegin_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT: {
        AV_SCOPE("server:vkCmdEndConditionalRenderingEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        s.on_vkCmdEndConditionalRenderingEXT(commandBuffer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT: {
        AV_SCOPE("server:vkCmdSetColorWriteEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t attachmentCount{};
        VkBool32* pColorWriteEnables_storage = nullptr;
        size_t pColorWriteEnables_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorWriteEnables_storage_count = static_cast<size_t>(attachmentCount);
        if (pColorWriteEnables_storage_count != 0) {
            pColorWriteEnables_storage = arena.allocate<VkBool32>(pColorWriteEnables_storage_count);
            if (pColorWriteEnables_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pColorWriteEnables_storage_count; ++av_z) { pColorWriteEnables_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pColorWriteEnables_storage_count; ++av_i) {
            if (!r.read_pod(pColorWriteEnables_storage[av_i])) return false;
        }
        s.on_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthBiasEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBiasEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasEnable)) return false;
        s.on_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLogicOpEXT: {
        AV_SCOPE("server:vkCmdSetLogicOpEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkLogicOp logicOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(logicOp)) return false;
        s.on_vkCmdSetLogicOpEXT(commandBuffer, logicOp);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT: {
        AV_SCOPE("server:vkCmdSetPatchControlPointsEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t patchControlPoints{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(patchControlPoints)) return false;
        s.on_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT: {
        AV_SCOPE("server:vkCmdSetPrimitiveRestartEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 primitiveRestartEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveRestartEnable)) return false;
        s.on_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT: {
        AV_SCOPE("server:vkCmdSetRasterizerDiscardEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 rasterizerDiscardEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizerDiscardEnable)) return false;
        s.on_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetVertexInputEXT: {
        AV_SCOPE("server:vkCmdSetVertexInputEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t vertexBindingDescriptionCount{};
        VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions_storage = nullptr;
        size_t pVertexBindingDescriptions_storage_count = 0;
        uint32_t vertexAttributeDescriptionCount{};
        VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions_storage = nullptr;
        size_t pVertexAttributeDescriptions_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(vertexBindingDescriptionCount)) return false;
        pVertexBindingDescriptions_storage_count = static_cast<size_t>(vertexBindingDescriptionCount);
        if (pVertexBindingDescriptions_storage_count != 0) {
            pVertexBindingDescriptions_storage = arena.allocate<VkVertexInputBindingDescription2EXT>(pVertexBindingDescriptions_storage_count);
            if (pVertexBindingDescriptions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pVertexBindingDescriptions_storage_count; ++av_z) { pVertexBindingDescriptions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pVertexBindingDescriptions_storage_count; ++av_i) {
            if (!deserialize_VkVertexInputBindingDescription2EXT(r, &pVertexBindingDescriptions_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(vertexAttributeDescriptionCount)) return false;
        pVertexAttributeDescriptions_storage_count = static_cast<size_t>(vertexAttributeDescriptionCount);
        if (pVertexAttributeDescriptions_storage_count != 0) {
            pVertexAttributeDescriptions_storage = arena.allocate<VkVertexInputAttributeDescription2EXT>(pVertexAttributeDescriptions_storage_count);
            if (pVertexAttributeDescriptions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pVertexAttributeDescriptions_storage_count; ++av_z) { pVertexAttributeDescriptions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pVertexAttributeDescriptions_storage_count; ++av_i) {
            if (!deserialize_VkVertexInputAttributeDescription2EXT(r, &pVertexAttributeDescriptions_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions_storage, vertexAttributeDescriptionCount, pVertexAttributeDescriptions_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkBuildAccelerationStructuresKHR: {
        AV_SCOPE("server:vkBuildAccelerationStructuresKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        uint32_t infoCount{};
        VkAccelerationStructureBuildGeometryInfoKHR* pInfos_storage = nullptr;
        size_t pInfos_storage_count = 0;
        const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos_outer = nullptr;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage_count = static_cast<size_t>(infoCount);
        if (pInfos_storage_count != 0) {
            pInfos_storage = arena.allocate<VkAccelerationStructureBuildGeometryInfoKHR>(pInfos_storage_count);
            if (pInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pInfos_storage_count; ++av_z) { pInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pInfos_storage_count; ++av_i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[av_i], arena)) return false;
        }
        {
            const uint32_t av_n_outer = static_cast<uint32_t>(infoCount);
            auto** av_ptrs = arena.allocate<const VkAccelerationStructureBuildRangeInfoKHR*>(av_n_outer);
            if (av_ptrs == nullptr) { return false; }
            for (uint32_t av_i = 0; av_i < av_n_outer; ++av_i) {
                const uint32_t av_n = static_cast<uint32_t>(pInfos_storage[av_i].geometryCount);
                auto* av_inner = arena.allocate<VkAccelerationStructureBuildRangeInfoKHR>(av_n);
                if (av_inner == nullptr) { return false; }
                for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
                    if (!deserialize_VkAccelerationStructureBuildRangeInfoKHR(r, &av_inner[av_j], arena)) return false;
                }
                av_ptrs[av_i] = av_inner;
            }
            ppBuildRangeInfos_outer = av_ptrs;
        }
        VkResult result = s.on_vkBuildAccelerationStructuresKHR(device, deferredOperation, infoCount, pInfos_storage, ppBuildRangeInfos_outer);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindShadersEXT: {
        AV_SCOPE("server:vkCmdBindShadersEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t stageCount{};
        VkShaderStageFlagBits* pStages_storage = nullptr;
        size_t pStages_storage_count = 0;
        VkShaderEXT* pShaders_storage = nullptr;
        size_t pShaders_storage_count = 0;
        uint8_t pShaders_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stageCount)) return false;
        pStages_storage_count = static_cast<size_t>(stageCount);
        if (pStages_storage_count != 0) {
            pStages_storage = arena.allocate<VkShaderStageFlagBits>(pStages_storage_count);
            if (pStages_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pStages_storage_count; ++av_z) { pStages_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pStages_storage_count; ++av_i) {
            if (!r.read_pod(pStages_storage[av_i])) return false;
        }
        if (!r.read_pod(pShaders_present)) return false;
        if (pShaders_present) {
            pShaders_storage_count = static_cast<size_t>(stageCount);
            if (pShaders_storage_count != 0) {
                pShaders_storage = arena.allocate<VkShaderEXT>(pShaders_storage_count);
                if (pShaders_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pShaders_storage_count; ++av_z) { pShaders_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pShaders_storage_count; ++av_i) {
                if (!deserialize_VkShaderEXT(r, pShaders_storage[av_i])) return false;
            }
        }
        s.on_vkCmdBindShadersEXT(commandBuffer, stageCount, pStages_storage, pShaders_present ? pShaders_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT: {
        AV_SCOPE("server:vkCmdBindVertexBuffers2EXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        VkBuffer* pBuffers_storage = nullptr;
        size_t pBuffers_storage_count = 0;
        VkDeviceSize* pOffsets_storage = nullptr;
        size_t pOffsets_storage_count = 0;
        VkDeviceSize* pSizes_storage = nullptr;
        size_t pSizes_storage_count = 0;
        uint8_t pSizes_present = 0;
        VkDeviceSize* pStrides_storage = nullptr;
        size_t pStrides_storage_count = 0;
        uint8_t pStrides_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage_count = static_cast<size_t>(bindingCount);
        if (pBuffers_storage_count != 0) {
            pBuffers_storage = arena.allocate<VkBuffer>(pBuffers_storage_count);
            if (pBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBuffers_storage_count; ++av_z) { pBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[av_i])) return false;
        }
        pOffsets_storage_count = static_cast<size_t>(bindingCount);
        if (pOffsets_storage_count != 0) {
            pOffsets_storage = arena.allocate<VkDeviceSize>(pOffsets_storage_count);
            if (pOffsets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pOffsets_storage_count; ++av_z) { pOffsets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pOffsets_storage_count; ++av_i) {
            if (!r.read_pod(pOffsets_storage[av_i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage_count = static_cast<size_t>(bindingCount);
            if (pSizes_storage_count != 0) {
                pSizes_storage = arena.allocate<VkDeviceSize>(pSizes_storage_count);
                if (pSizes_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSizes_storage_count; ++av_z) { pSizes_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pSizes_storage_count; ++av_i) {
                if (!r.read_pod(pSizes_storage[av_i])) return false;
            }
        }
        if (!r.read_pod(pStrides_present)) return false;
        if (pStrides_present) {
            pStrides_storage_count = static_cast<size_t>(bindingCount);
            if (pStrides_storage_count != 0) {
                pStrides_storage = arena.allocate<VkDeviceSize>(pStrides_storage_count);
                if (pStrides_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pStrides_storage_count; ++av_z) { pStrides_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pStrides_storage_count; ++av_i) {
                if (!r.read_pod(pStrides_storage[av_i])) return false;
            }
        }
        s.on_vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers_storage, pOffsets_storage, pSizes_present ? pSizes_storage : nullptr, pStrides_present ? pStrides_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR: {
        AV_SCOPE("server:vkCmdBuildAccelerationStructuresIndirectKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t infoCount{};
        VkAccelerationStructureBuildGeometryInfoKHR* pInfos_storage = nullptr;
        size_t pInfos_storage_count = 0;
        VkDeviceAddress* pIndirectDeviceAddresses_storage = nullptr;
        size_t pIndirectDeviceAddresses_storage_count = 0;
        uint32_t* pIndirectStrides_storage = nullptr;
        size_t pIndirectStrides_storage_count = 0;
        const uint32_t** ppMaxPrimitiveCounts_outer = nullptr;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage_count = static_cast<size_t>(infoCount);
        if (pInfos_storage_count != 0) {
            pInfos_storage = arena.allocate<VkAccelerationStructureBuildGeometryInfoKHR>(pInfos_storage_count);
            if (pInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pInfos_storage_count; ++av_z) { pInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pInfos_storage_count; ++av_i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[av_i], arena)) return false;
        }
        pIndirectDeviceAddresses_storage_count = static_cast<size_t>(infoCount);
        if (pIndirectDeviceAddresses_storage_count != 0) {
            pIndirectDeviceAddresses_storage = arena.allocate<VkDeviceAddress>(pIndirectDeviceAddresses_storage_count);
            if (pIndirectDeviceAddresses_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pIndirectDeviceAddresses_storage_count; ++av_z) { pIndirectDeviceAddresses_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pIndirectDeviceAddresses_storage_count; ++av_i) {
            if (!r.read_pod(pIndirectDeviceAddresses_storage[av_i])) return false;
        }
        pIndirectStrides_storage_count = static_cast<size_t>(infoCount);
        if (pIndirectStrides_storage_count != 0) {
            pIndirectStrides_storage = arena.allocate<uint32_t>(pIndirectStrides_storage_count);
            if (pIndirectStrides_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pIndirectStrides_storage_count; ++av_z) { pIndirectStrides_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pIndirectStrides_storage_count; ++av_i) {
            if (!r.read_pod(pIndirectStrides_storage[av_i])) return false;
        }
        {
            const uint32_t av_n_outer = static_cast<uint32_t>(infoCount);
            auto** av_ptrs = arena.allocate<const uint32_t*>(av_n_outer);
            if (av_ptrs == nullptr) { return false; }
            for (uint32_t av_i = 0; av_i < av_n_outer; ++av_i) {
                const uint32_t av_n = static_cast<uint32_t>(pInfos_storage[av_i].geometryCount);
                auto* av_inner = arena.allocate<uint32_t>(av_n);
                if (av_inner == nullptr) { return false; }
                for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
                    if (!r.read_pod(av_inner[av_j])) return false;
                }
                av_ptrs[av_i] = av_inner;
            }
            ppMaxPrimitiveCounts_outer = av_ptrs;
        }
        s.on_vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos_storage, pIndirectDeviceAddresses_storage, pIndirectStrides_storage, ppMaxPrimitiveCounts_outer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR: {
        AV_SCOPE("server:vkCmdBuildAccelerationStructuresKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t infoCount{};
        VkAccelerationStructureBuildGeometryInfoKHR* pInfos_storage = nullptr;
        size_t pInfos_storage_count = 0;
        const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos_outer = nullptr;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage_count = static_cast<size_t>(infoCount);
        if (pInfos_storage_count != 0) {
            pInfos_storage = arena.allocate<VkAccelerationStructureBuildGeometryInfoKHR>(pInfos_storage_count);
            if (pInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pInfos_storage_count; ++av_z) { pInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pInfos_storage_count; ++av_i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[av_i], arena)) return false;
        }
        {
            const uint32_t av_n_outer = static_cast<uint32_t>(infoCount);
            auto** av_ptrs = arena.allocate<const VkAccelerationStructureBuildRangeInfoKHR*>(av_n_outer);
            if (av_ptrs == nullptr) { return false; }
            for (uint32_t av_i = 0; av_i < av_n_outer; ++av_i) {
                const uint32_t av_n = static_cast<uint32_t>(pInfos_storage[av_i].geometryCount);
                auto* av_inner = arena.allocate<VkAccelerationStructureBuildRangeInfoKHR>(av_n);
                if (av_inner == nullptr) { return false; }
                for (uint32_t av_j = 0; av_j < av_n; ++av_j) {
                    if (!deserialize_VkAccelerationStructureBuildRangeInfoKHR(r, &av_inner[av_j], arena)) return false;
                }
                av_ptrs[av_i] = av_inner;
            }
            ppBuildRangeInfos_outer = av_ptrs;
        }
        s.on_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos_storage, ppBuildRangeInfos_outer);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR: {
        AV_SCOPE("server:vkCmdCopyAccelerationStructureKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        s.on_vkCmdCopyAccelerationStructureKHR(commandBuffer, &pInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR: {
        AV_SCOPE("server:vkCmdCopyAccelerationStructureToMemoryKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyAccelerationStructureToMemoryInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyAccelerationStructureToMemoryInfoKHR(r, &pInfo_local, arena)) return false;
        s.on_vkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer, &pInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR: {
        AV_SCOPE("server:vkCmdCopyMemoryToAccelerationStructureKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyMemoryToAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyMemoryToAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        s.on_vkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer, &pInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT: {
        AV_SCOPE("server:vkCmdDrawMeshTasksEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        s.on_vkCmdDrawMeshTasksEXT(commandBuffer, groupCountX, groupCountY, groupCountZ);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT: {
        AV_SCOPE("server:vkCmdDrawMeshTasksIndirectCountEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (countBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT: {
        AV_SCOPE("server:vkCmdDrawMeshTasksIndirectEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (buffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawMeshTasksIndirectEXT(commandBuffer, buffer, offset, drawCount, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR: {
        AV_SCOPE("server:vkCmdPushDescriptorSetKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipelineLayout layout = VK_NULL_HANDLE;
        uint32_t set{};
        uint32_t descriptorWriteCount{};
        VkWriteDescriptorSet* pDescriptorWrites_storage = nullptr;
        size_t pDescriptorWrites_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (layout == VK_NULL_HANDLE) return false;
        if (!r.read_pod(set)) return false;
        if (!r.read_pod(descriptorWriteCount)) return false;
        pDescriptorWrites_storage_count = static_cast<size_t>(descriptorWriteCount);
        if (pDescriptorWrites_storage_count != 0) {
            pDescriptorWrites_storage = arena.allocate<VkWriteDescriptorSet>(pDescriptorWrites_storage_count);
            if (pDescriptorWrites_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pDescriptorWrites_storage_count; ++av_z) { pDescriptorWrites_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pDescriptorWrites_storage_count; ++av_i) {
            if (!deserialize_VkWriteDescriptorSet(r, &pDescriptorWrites_storage[av_i], arena)) return false;
        }
        s.on_vkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT: {
        AV_SCOPE("server:vkCmdSetAlphaToCoverageEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 alphaToCoverageEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(alphaToCoverageEnable)) return false;
        s.on_vkCmdSetAlphaToCoverageEnableEXT(commandBuffer, alphaToCoverageEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT: {
        AV_SCOPE("server:vkCmdSetAlphaToOneEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 alphaToOneEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(alphaToOneEnable)) return false;
        s.on_vkCmdSetAlphaToOneEnableEXT(commandBuffer, alphaToOneEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT: {
        AV_SCOPE("server:vkCmdSetColorBlendAdvancedEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        VkColorBlendAdvancedEXT* pColorBlendAdvanced_storage = nullptr;
        size_t pColorBlendAdvanced_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendAdvanced_storage_count = static_cast<size_t>(attachmentCount);
        if (pColorBlendAdvanced_storage_count != 0) {
            pColorBlendAdvanced_storage = arena.allocate<VkColorBlendAdvancedEXT>(pColorBlendAdvanced_storage_count);
            if (pColorBlendAdvanced_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pColorBlendAdvanced_storage_count; ++av_z) { pColorBlendAdvanced_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pColorBlendAdvanced_storage_count; ++av_i) {
            if (!deserialize_VkColorBlendAdvancedEXT(r, &pColorBlendAdvanced_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetColorBlendAdvancedEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT: {
        AV_SCOPE("server:vkCmdSetColorBlendEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        VkBool32* pColorBlendEnables_storage = nullptr;
        size_t pColorBlendEnables_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendEnables_storage_count = static_cast<size_t>(attachmentCount);
        if (pColorBlendEnables_storage_count != 0) {
            pColorBlendEnables_storage = arena.allocate<VkBool32>(pColorBlendEnables_storage_count);
            if (pColorBlendEnables_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pColorBlendEnables_storage_count; ++av_z) { pColorBlendEnables_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pColorBlendEnables_storage_count; ++av_i) {
            if (!r.read_pod(pColorBlendEnables_storage[av_i])) return false;
        }
        s.on_vkCmdSetColorBlendEnableEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT: {
        AV_SCOPE("server:vkCmdSetColorBlendEquationEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        VkColorBlendEquationEXT* pColorBlendEquations_storage = nullptr;
        size_t pColorBlendEquations_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendEquations_storage_count = static_cast<size_t>(attachmentCount);
        if (pColorBlendEquations_storage_count != 0) {
            pColorBlendEquations_storage = arena.allocate<VkColorBlendEquationEXT>(pColorBlendEquations_storage_count);
            if (pColorBlendEquations_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pColorBlendEquations_storage_count; ++av_z) { pColorBlendEquations_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pColorBlendEquations_storage_count; ++av_i) {
            if (!deserialize_VkColorBlendEquationEXT(r, &pColorBlendEquations_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetColorBlendEquationEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT: {
        AV_SCOPE("server:vkCmdSetColorWriteMaskEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        VkColorComponentFlags* pColorWriteMasks_storage = nullptr;
        size_t pColorWriteMasks_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorWriteMasks_storage_count = static_cast<size_t>(attachmentCount);
        if (pColorWriteMasks_storage_count != 0) {
            pColorWriteMasks_storage = arena.allocate<VkColorComponentFlags>(pColorWriteMasks_storage_count);
            if (pColorWriteMasks_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pColorWriteMasks_storage_count; ++av_z) { pColorWriteMasks_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pColorWriteMasks_storage_count; ++av_i) {
            if (!r.read_pod(pColorWriteMasks_storage[av_i])) return false;
        }
        s.on_vkCmdSetColorWriteMaskEXT(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT: {
        AV_SCOPE("server:vkCmdSetConservativeRasterizationModeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkConservativeRasterizationModeEXT conservativeRasterizationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(conservativeRasterizationMode)) return false;
        s.on_vkCmdSetConservativeRasterizationModeEXT(commandBuffer, conservativeRasterizationMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV: {
        AV_SCOPE("server:vkCmdSetCoverageModulationModeNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCoverageModulationModeNV coverageModulationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationMode)) return false;
        s.on_vkCmdSetCoverageModulationModeNV(commandBuffer, coverageModulationMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV: {
        AV_SCOPE("server:vkCmdSetCoverageModulationTableEnableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 coverageModulationTableEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationTableEnable)) return false;
        s.on_vkCmdSetCoverageModulationTableEnableNV(commandBuffer, coverageModulationTableEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV: {
        AV_SCOPE("server:vkCmdSetCoverageModulationTableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t coverageModulationTableCount{};
        float* pCoverageModulationTable_storage = nullptr;
        size_t pCoverageModulationTable_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationTableCount)) return false;
        pCoverageModulationTable_storage_count = static_cast<size_t>(coverageModulationTableCount);
        if (pCoverageModulationTable_storage_count != 0) {
            pCoverageModulationTable_storage = arena.allocate<float>(pCoverageModulationTable_storage_count);
            if (pCoverageModulationTable_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCoverageModulationTable_storage_count; ++av_z) { pCoverageModulationTable_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCoverageModulationTable_storage_count; ++av_i) {
            if (!r.read_pod(pCoverageModulationTable_storage[av_i])) return false;
        }
        s.on_vkCmdSetCoverageModulationTableNV(commandBuffer, coverageModulationTableCount, pCoverageModulationTable_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV: {
        AV_SCOPE("server:vkCmdSetCoverageReductionModeNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCoverageReductionModeNV coverageReductionMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageReductionMode)) return false;
        s.on_vkCmdSetCoverageReductionModeNV(commandBuffer, coverageReductionMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV: {
        AV_SCOPE("server:vkCmdSetCoverageToColorEnableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 coverageToColorEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageToColorEnable)) return false;
        s.on_vkCmdSetCoverageToColorEnableNV(commandBuffer, coverageToColorEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV: {
        AV_SCOPE("server:vkCmdSetCoverageToColorLocationNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t coverageToColorLocation{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageToColorLocation)) return false;
        s.on_vkCmdSetCoverageToColorLocationNV(commandBuffer, coverageToColorLocation);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCullModeEXT: {
        AV_SCOPE("server:vkCmdSetCullModeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCullModeFlags cullMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(cullMode)) return false;
        s.on_vkCmdSetCullModeEXT(commandBuffer, cullMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthBoundsTestEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBoundsTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBoundsTestEnable)) return false;
        s.on_vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthClampEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthClampEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClampEnable)) return false;
        s.on_vkCmdSetDepthClampEnableEXT(commandBuffer, depthClampEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT: {
        AV_SCOPE("server:vkCmdSetDepthClampRangeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDepthClampModeEXT depthClampMode{};
        VkDepthClampRangeEXT pDepthClampRange_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClampMode)) return false;
        if (!deserialize_VkDepthClampRangeEXT(r, &pDepthClampRange_local, arena)) return false;
        s.on_vkCmdSetDepthClampRangeEXT(commandBuffer, depthClampMode, &pDepthClampRange_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthClipEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthClipEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClipEnable)) return false;
        s.on_vkCmdSetDepthClipEnableEXT(commandBuffer, depthClipEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT: {
        AV_SCOPE("server:vkCmdSetDepthClipNegativeOneToOneEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 negativeOneToOne{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(negativeOneToOne)) return false;
        s.on_vkCmdSetDepthClipNegativeOneToOneEXT(commandBuffer, negativeOneToOne);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT: {
        AV_SCOPE("server:vkCmdSetDepthCompareOpEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCompareOp depthCompareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthCompareOp)) return false;
        s.on_vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthTestEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthTestEnable)) return false;
        s.on_vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT: {
        AV_SCOPE("server:vkCmdSetDepthWriteEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthWriteEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthWriteEnable)) return false;
        s.on_vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT: {
        AV_SCOPE("server:vkCmdSetExtraPrimitiveOverestimationSizeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float extraPrimitiveOverestimationSize{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(extraPrimitiveOverestimationSize)) return false;
        s.on_vkCmdSetExtraPrimitiveOverestimationSizeEXT(commandBuffer, extraPrimitiveOverestimationSize);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR: {
        AV_SCOPE("server:vkCmdSetFragmentShadingRateKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkExtent2D pFragmentSize_local{};
        VkFragmentShadingRateCombinerOpKHR combinerOps_local[2]{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkExtent2D(r, &pFragmentSize_local, arena)) return false;
        for (uint32_t av_i = 0; av_i < 2u; ++av_i) {
            if (!r.read_pod(combinerOps_local[av_i])) return false;
        }
        s.on_vkCmdSetFragmentShadingRateKHR(commandBuffer, &pFragmentSize_local, combinerOps_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFrontFaceEXT: {
        AV_SCOPE("server:vkCmdSetFrontFaceEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFrontFace frontFace{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(frontFace)) return false;
        s.on_vkCmdSetFrontFaceEXT(commandBuffer, frontFace);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT: {
        AV_SCOPE("server:vkCmdSetLineRasterizationModeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkLineRasterizationModeEXT lineRasterizationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineRasterizationMode)) return false;
        s.on_vkCmdSetLineRasterizationModeEXT(commandBuffer, lineRasterizationMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineStippleEXT: {
        AV_SCOPE("server:vkCmdSetLineStippleEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t lineStippleFactor{};
        uint16_t lineStipplePattern{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineStippleFactor)) return false;
        if (!r.read_pod(lineStipplePattern)) return false;
        s.on_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT: {
        AV_SCOPE("server:vkCmdSetLineStippleEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stippledLineEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stippledLineEnable)) return false;
        s.on_vkCmdSetLineStippleEnableEXT(commandBuffer, stippledLineEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT: {
        AV_SCOPE("server:vkCmdSetLogicOpEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 logicOpEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(logicOpEnable)) return false;
        s.on_vkCmdSetLogicOpEnableEXT(commandBuffer, logicOpEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPolygonModeEXT: {
        AV_SCOPE("server:vkCmdSetPolygonModeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPolygonMode polygonMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(polygonMode)) return false;
        s.on_vkCmdSetPolygonModeEXT(commandBuffer, polygonMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT: {
        AV_SCOPE("server:vkCmdSetPrimitiveTopologyEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPrimitiveTopology primitiveTopology{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveTopology)) return false;
        s.on_vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT: {
        AV_SCOPE("server:vkCmdSetProvokingVertexModeEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkProvokingVertexModeEXT provokingVertexMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(provokingVertexMode)) return false;
        s.on_vkCmdSetProvokingVertexModeEXT(commandBuffer, provokingVertexMode);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT: {
        AV_SCOPE("server:vkCmdSetRasterizationSamplesEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSampleCountFlagBits rasterizationSamples{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizationSamples)) return false;
        s.on_vkCmdSetRasterizationSamplesEXT(commandBuffer, rasterizationSamples);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT: {
        AV_SCOPE("server:vkCmdSetRasterizationStreamEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t rasterizationStream{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizationStream)) return false;
        s.on_vkCmdSetRasterizationStreamEXT(commandBuffer, rasterizationStream);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR: {
        AV_SCOPE("server:vkCmdSetRayTracingPipelineStackSizeKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t pipelineStackSize{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineStackSize)) return false;
        s.on_vkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer, pipelineStackSize);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV: {
        AV_SCOPE("server:vkCmdSetRepresentativeFragmentTestEnableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 representativeFragmentTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(representativeFragmentTestEnable)) return false;
        s.on_vkCmdSetRepresentativeFragmentTestEnableNV(commandBuffer, representativeFragmentTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT: {
        AV_SCOPE("server:vkCmdSetSampleLocationsEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 sampleLocationsEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(sampleLocationsEnable)) return false;
        s.on_vkCmdSetSampleLocationsEnableEXT(commandBuffer, sampleLocationsEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetSampleMaskEXT: {
        AV_SCOPE("server:vkCmdSetSampleMaskEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSampleCountFlagBits samples{};
        VkSampleMask* pSampleMask_storage = nullptr;
        size_t pSampleMask_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(samples)) return false;
        pSampleMask_storage_count = static_cast<size_t>((samples + 31) / 32);
        if (pSampleMask_storage_count != 0) {
            pSampleMask_storage = arena.allocate<VkSampleMask>(pSampleMask_storage_count);
            if (pSampleMask_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pSampleMask_storage_count; ++av_z) { pSampleMask_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pSampleMask_storage_count; ++av_i) {
            if (!r.read_pod(pSampleMask_storage[av_i])) return false;
        }
        s.on_vkCmdSetSampleMaskEXT(commandBuffer, samples, pSampleMask_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT: {
        AV_SCOPE("server:vkCmdSetScissorWithCountEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t scissorCount{};
        VkRect2D* pScissors_storage = nullptr;
        size_t pScissors_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage_count = static_cast<size_t>(scissorCount);
        if (pScissors_storage_count != 0) {
            pScissors_storage = arena.allocate<VkRect2D>(pScissors_storage_count);
            if (pScissors_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pScissors_storage_count; ++av_z) { pScissors_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pScissors_storage_count; ++av_i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV: {
        AV_SCOPE("server:vkCmdSetShadingRateImageEnableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 shadingRateImageEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(shadingRateImageEnable)) return false;
        s.on_vkCmdSetShadingRateImageEnableNV(commandBuffer, shadingRateImageEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilOpEXT: {
        AV_SCOPE("server:vkCmdSetStencilOpEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        VkStencilOp failOp{};
        VkStencilOp passOp{};
        VkStencilOp depthFailOp{};
        VkCompareOp compareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(failOp)) return false;
        if (!r.read_pod(passOp)) return false;
        if (!r.read_pod(depthFailOp)) return false;
        if (!r.read_pod(compareOp)) return false;
        s.on_vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT: {
        AV_SCOPE("server:vkCmdSetStencilTestEnableEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stencilTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stencilTestEnable)) return false;
        s.on_vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT: {
        AV_SCOPE("server:vkCmdSetTessellationDomainOriginEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkTessellationDomainOrigin domainOrigin{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(domainOrigin)) return false;
        s.on_vkCmdSetTessellationDomainOriginEXT(commandBuffer, domainOrigin);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV: {
        AV_SCOPE("server:vkCmdSetViewportSwizzleNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstViewport{};
        uint32_t viewportCount{};
        VkViewportSwizzleNV* pViewportSwizzles_storage = nullptr;
        size_t pViewportSwizzles_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstViewport)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewportSwizzles_storage_count = static_cast<size_t>(viewportCount);
        if (pViewportSwizzles_storage_count != 0) {
            pViewportSwizzles_storage = arena.allocate<VkViewportSwizzleNV>(pViewportSwizzles_storage_count);
            if (pViewportSwizzles_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pViewportSwizzles_storage_count; ++av_z) { pViewportSwizzles_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pViewportSwizzles_storage_count; ++av_i) {
            if (!deserialize_VkViewportSwizzleNV(r, &pViewportSwizzles_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetViewportSwizzleNV(commandBuffer, firstViewport, viewportCount, pViewportSwizzles_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV: {
        AV_SCOPE("server:vkCmdSetViewportWScalingEnableNV");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 viewportWScalingEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportWScalingEnable)) return false;
        s.on_vkCmdSetViewportWScalingEnableNV(commandBuffer, viewportWScalingEnable);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT: {
        AV_SCOPE("server:vkCmdSetViewportWithCountEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t viewportCount{};
        VkViewport* pViewports_storage = nullptr;
        size_t pViewports_storage_count = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage_count = static_cast<size_t>(viewportCount);
        if (pViewports_storage_count != 0) {
            pViewports_storage = arena.allocate<VkViewport>(pViewports_storage_count);
            if (pViewports_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pViewports_storage_count; ++av_z) { pViewports_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pViewports_storage_count; ++av_i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[av_i], arena)) return false;
        }
        s.on_vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports_storage);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR: {
        AV_SCOPE("server:vkCmdTraceRaysIndirectKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR pRaygenShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pMissShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pHitShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pCallableShaderBindingTable_local{};
        VkDeviceAddress indirectDeviceAddress{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pRaygenShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pMissShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pHitShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pCallableShaderBindingTable_local, arena)) return false;
        if (!r.read_pod(indirectDeviceAddress)) return false;
        s.on_vkCmdTraceRaysIndirectKHR(commandBuffer, &pRaygenShaderBindingTable_local, &pMissShaderBindingTable_local, &pHitShaderBindingTable_local, &pCallableShaderBindingTable_local, indirectDeviceAddress);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdTraceRaysKHR: {
        AV_SCOPE("server:vkCmdTraceRaysKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR pRaygenShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pMissShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pHitShaderBindingTable_local{};
        VkStridedDeviceAddressRegionKHR pCallableShaderBindingTable_local{};
        uint32_t width{};
        uint32_t height{};
        uint32_t depth{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pRaygenShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pMissShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pHitShaderBindingTable_local, arena)) return false;
        if (!deserialize_VkStridedDeviceAddressRegionKHR(r, &pCallableShaderBindingTable_local, arena)) return false;
        if (!r.read_pod(width)) return false;
        if (!r.read_pod(height)) return false;
        if (!r.read_pod(depth)) return false;
        s.on_vkCmdTraceRaysKHR(commandBuffer, &pRaygenShaderBindingTable_local, &pMissShaderBindingTable_local, &pHitShaderBindingTable_local, &pCallableShaderBindingTable_local, width, height, depth);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR: {
        AV_SCOPE("server:vkCmdWriteAccelerationStructuresPropertiesKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t accelerationStructureCount{};
        VkAccelerationStructureKHR* pAccelerationStructures_storage = nullptr;
        size_t pAccelerationStructures_storage_count = 0;
        VkQueryType queryType{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(accelerationStructureCount)) return false;
        pAccelerationStructures_storage_count = static_cast<size_t>(accelerationStructureCount);
        if (pAccelerationStructures_storage_count != 0) {
            pAccelerationStructures_storage = arena.allocate<VkAccelerationStructureKHR>(pAccelerationStructures_storage_count);
            if (pAccelerationStructures_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pAccelerationStructures_storage_count; ++av_z) { pAccelerationStructures_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pAccelerationStructures_storage_count; ++av_i) {
            if (!deserialize_VkAccelerationStructureKHR(r, pAccelerationStructures_storage[av_i])) return false;
        }
        if (!r.read_pod(queryType)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstQuery)) return false;
        s.on_vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, accelerationStructureCount, pAccelerationStructures_storage, queryType, queryPool, firstQuery);
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyAccelerationStructureKHR: {
        AV_SCOPE("server:vkCopyAccelerationStructureKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyAccelerationStructureKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR: {
        AV_SCOPE("server:vkCopyAccelerationStructureToMemoryKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyAccelerationStructureToMemoryInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyAccelerationStructureToMemoryInfoKHR(r, &pInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyAccelerationStructureToMemoryKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyImageToImageEXT: {
        AV_SCOPE("server:vkCopyImageToImageEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkCopyImageToImageInfo pCopyImageToImageInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyImageToImageInfo(r, &pCopyImageToImageInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyImageToImageEXT(device, &pCopyImageToImageInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyImageToMemoryEXT: {
        AV_SCOPE("server:vkCopyImageToMemoryEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkCopyImageToMemoryInfo pCopyImageToMemoryInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyImageToMemoryInfo(r, &pCopyImageToMemoryInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyImageToMemoryEXT(device, &pCopyImageToMemoryInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR: {
        AV_SCOPE("server:vkCopyMemoryToAccelerationStructureKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyMemoryToAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyMemoryToAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyMemoryToAccelerationStructureKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyMemoryToImageEXT: {
        AV_SCOPE("server:vkCopyMemoryToImageEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkCopyMemoryToImageInfo pCopyMemoryToImageInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyMemoryToImageInfo(r, &pCopyMemoryToImageInfo_local, arena)) return false;
        VkResult result = s.on_vkCopyMemoryToImageEXT(device, &pCopyMemoryToImageInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateAccelerationStructureKHR: {
        AV_SCOPE("server:vkCreateAccelerationStructureKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR pCreateInfo_local{};
        VkAccelerationStructureKHR pAccelerationStructure_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureCreateInfoKHR(r, &pCreateInfo_local, arena)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateAccelerationStructureKHR(device, &pCreateInfo_local, nullptr, &pAccelerationStructure_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pAccelerationStructure_out = VK_NULL_HANDLE;
        }
        serialize_VkAccelerationStructureKHR(reply, pAccelerationStructure_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDeferredOperationKHR: {
        AV_SCOPE("server:vkCreateDeferredOperationKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR pDeferredOperation_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        VkResult result = handles_room(static_cast<uint64_t>(1u)) ? s.on_vkCreateDeferredOperationKHR(device, nullptr, &pDeferredOperation_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pDeferredOperation_out = VK_NULL_HANDLE;
        }
        serialize_VkDeferredOperationKHR(reply, pDeferredOperation_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR: {
        AV_SCOPE("server:vkCreateRayTracingPipelinesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        VkRayTracingPipelineCreateInfoKHR* pCreateInfos_storage = nullptr;
        size_t pCreateInfos_storage_count = 0;
        VkPipeline* pPipelines_storage = nullptr;
        size_t pPipelines_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage_count = static_cast<size_t>(createInfoCount);
        if (pCreateInfos_storage_count != 0) {
            pCreateInfos_storage = arena.allocate<VkRayTracingPipelineCreateInfoKHR>(pCreateInfos_storage_count);
            if (pCreateInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCreateInfos_storage_count; ++av_z) { pCreateInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCreateInfos_storage_count; ++av_i) {
            if (!deserialize_VkRayTracingPipelineCreateInfoKHR(r, &pCreateInfos_storage[av_i], arena)) return false;
        }
        pPipelines_storage_count = static_cast<size_t>(createInfoCount);
        if (pPipelines_storage_count != 0) {
            pPipelines_storage = arena.allocate<VkPipeline>(pPipelines_storage_count);
            if (pPipelines_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pPipelines_storage_count)) ? s.on_vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos_storage, nullptr, pPipelines_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pPipelines_storage_count; ++av_z) { pPipelines_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pPipelines_storage_count; ++av_k) {
            serialize_VkPipeline(reply, pPipelines_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateShadersEXT: {
        AV_SCOPE("server:vkCreateShadersEXT");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        VkShaderCreateInfoEXT* pCreateInfos_storage = nullptr;
        size_t pCreateInfos_storage_count = 0;
        VkShaderEXT* pShaders_storage = nullptr;
        size_t pShaders_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage_count = static_cast<size_t>(createInfoCount);
        if (pCreateInfos_storage_count != 0) {
            pCreateInfos_storage = arena.allocate<VkShaderCreateInfoEXT>(pCreateInfos_storage_count);
            if (pCreateInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCreateInfos_storage_count; ++av_z) { pCreateInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCreateInfos_storage_count; ++av_i) {
            if (!deserialize_VkShaderCreateInfoEXT(r, &pCreateInfos_storage[av_i], arena)) return false;
        }
        pShaders_storage_count = static_cast<size_t>(createInfoCount);
        if (pShaders_storage_count != 0) {
            pShaders_storage = arena.allocate<VkShaderEXT>(pShaders_storage_count);
            if (pShaders_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pShaders_storage_count; ++av_z) { pShaders_storage[av_z] = {}; }
        }
        VkResult result = handles_room(static_cast<uint64_t>(pShaders_storage_count)) ? s.on_vkCreateShadersEXT(device, createInfoCount, pCreateInfos_storage, nullptr, pShaders_storage) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pShaders_storage_count; ++av_z) { pShaders_storage[av_z] = {}; }
        }
        for (size_t av_k = 0; av_k < pShaders_storage_count; ++av_k) {
            serialize_VkShaderEXT(reply, pShaders_storage[av_k]);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkDeferredOperationJoinKHR: {
        AV_SCOPE("server:vkDeferredOperationJoinKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        if (operation == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkDeferredOperationJoinKHR(device, operation);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR: {
        AV_SCOPE("server:vkDestroyAccelerationStructureKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureKHR(r, accelerationStructure)) return false;
        s.on_vkDestroyAccelerationStructureKHR(device, accelerationStructure, nullptr);
        handle_ended(as_number(accelerationStructure));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDeferredOperationKHR: {
        AV_SCOPE("server:vkDestroyDeferredOperationKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        s.on_vkDestroyDeferredOperationKHR(device, operation, nullptr);
        handle_ended(as_number(operation));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyShaderEXT: {
        AV_SCOPE("server:vkDestroyShaderEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkShaderEXT shader = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderEXT(r, shader)) return false;
        s.on_vkDestroyShaderEXT(device, shader, nullptr);
        handle_ended(as_number(shader));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR: {
        AV_SCOPE("server:vkGetAccelerationStructureBuildSizesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureBuildTypeKHR buildType{};
        VkAccelerationStructureBuildGeometryInfoKHR pBuildInfo_local{};
        uint32_t* pMaxPrimitiveCounts_storage = nullptr;
        size_t pMaxPrimitiveCounts_storage_count = 0;
        uint8_t pMaxPrimitiveCounts_present = 0;
        VkAccelerationStructureBuildSizesInfoKHR pSizeInfo_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(buildType)) return false;
        if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pBuildInfo_local, arena)) return false;
        if (!r.read_pod(pMaxPrimitiveCounts_present)) return false;
        if (pMaxPrimitiveCounts_present) {
            pMaxPrimitiveCounts_storage_count = static_cast<size_t>(pBuildInfo_local.geometryCount);
            if (pMaxPrimitiveCounts_storage_count != 0) {
                pMaxPrimitiveCounts_storage = arena.allocate<uint32_t>(pMaxPrimitiveCounts_storage_count);
                if (pMaxPrimitiveCounts_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pMaxPrimitiveCounts_storage_count; ++av_z) { pMaxPrimitiveCounts_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pMaxPrimitiveCounts_storage_count; ++av_i) {
                if (!r.read_pod(pMaxPrimitiveCounts_storage[av_i])) return false;
            }
        }
        if (!r.read_pod(pSizeInfo_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSizeInfo_out), arena)) return false;
        s.on_vkGetAccelerationStructureBuildSizesKHR(device, buildType, &pBuildInfo_local, pMaxPrimitiveCounts_present ? pMaxPrimitiveCounts_storage : nullptr, &pSizeInfo_out);
        reply.write_pod<int32_t>(0);
        serialize_VkAccelerationStructureBuildSizesInfoKHR(reply, pSizeInfo_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetAccelerationStructureDeviceAddressKHR: {
        AV_SCOPE("server:vkGetAccelerationStructureDeviceAddressKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureDeviceAddressInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureDeviceAddressInfoKHR(r, &pInfo_local, arena)) return false;
        VkDeviceAddress result = s.on_vkGetAccelerationStructureDeviceAddressKHR(device, &pInfo_local);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeferredOperationMaxConcurrencyKHR: {
        AV_SCOPE("server:vkGetDeferredOperationMaxConcurrencyKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        if (operation == VK_NULL_HANDLE) return false;
        uint32_t result = s.on_vkGetDeferredOperationMaxConcurrencyKHR(device, operation);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeferredOperationResultKHR: {
        AV_SCOPE("server:vkGetDeferredOperationResultKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        if (operation == VK_NULL_HANDLE) return false;
        VkResult result = s.on_vkGetDeferredOperationResultKHR(device, operation);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceAccelerationStructureCompatibilityKHR: {
        AV_SCOPE("server:vkGetDeviceAccelerationStructureCompatibilityKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureVersionInfoKHR pVersionInfo_local{};
        VkAccelerationStructureCompatibilityKHR pCompatibility_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureVersionInfoKHR(r, &pVersionInfo_local, arena)) return false;
        s.on_vkGetDeviceAccelerationStructureCompatibilityKHR(device, &pVersionInfo_local, &pCompatibility_out);
        reply.write_pod<int32_t>(0);
        reply.write_pod(pCompatibility_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT: {
        AV_SCOPE("server:vkGetImageSubresourceLayout2EXT");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource2 pSubresource_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!deserialize_VkImageSubresource2(r, &pSubresource_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        s.on_vkGetImageSubresourceLayout2EXT(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFragmentShadingRatesKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceFragmentShadingRatesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pFragmentShadingRateCount_local = 0;
        VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates_storage = nullptr;
        size_t pFragmentShadingRates_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pFragmentShadingRateCount_local)) return false;
            pFragmentShadingRates_storage_count = static_cast<size_t>(pFragmentShadingRateCount_local);
            if (pFragmentShadingRates_storage_count != 0) {
                pFragmentShadingRates_storage = arena.allocate<VkPhysicalDeviceFragmentShadingRateKHR>(pFragmentShadingRates_storage_count);
                if (pFragmentShadingRates_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pFragmentShadingRates_storage_count; ++av_z) { pFragmentShadingRates_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pFragmentShadingRates_storage_count; ++av_e) { pFragmentShadingRates_storage[av_e].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR; pFragmentShadingRates_storage[av_e].pNext = nullptr; }
        }
        VkResult result = s.on_vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &pFragmentShadingRateCount_local, array_present ? pFragmentShadingRates_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pFragmentShadingRateCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pFragmentShadingRateCount_local;
        if (array_present && pFragmentShadingRateCount_sent > pFragmentShadingRates_storage_count) {
            pFragmentShadingRateCount_sent = static_cast<uint32_t>(pFragmentShadingRates_storage_count);
        }
        reply.write_pod(pFragmentShadingRateCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pFragmentShadingRateCount_sent; ++k) {
                serialize_VkPhysicalDeviceFragmentShadingRateKHR(reply, pFragmentShadingRates_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR: {
        AV_SCOPE("server:vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t firstGroup{};
        uint32_t groupCount{};
        size_t dataSize{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (pipeline == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstGroup)) return false;
        if (!r.read_pod(groupCount)) return false;
        {
            uint64_t av_wide = 0;
            if (!r.read_pod(av_wide)) return false;
            dataSize = static_cast<size_t>(av_wide);
        }
        pData_storage_count = static_cast<size_t>(dataSize);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        VkResult result = s.on_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        if (pData_storage_count != 0) {
            reply.write(pData_storage, pData_storage_count);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingShaderGroupHandlesKHR: {
        AV_SCOPE("server:vkGetRayTracingShaderGroupHandlesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t firstGroup{};
        uint32_t groupCount{};
        size_t dataSize{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (pipeline == VK_NULL_HANDLE) return false;
        if (!r.read_pod(firstGroup)) return false;
        if (!r.read_pod(groupCount)) return false;
        {
            uint64_t av_wide = 0;
            if (!r.read_pod(av_wide)) return false;
            dataSize = static_cast<size_t>(av_wide);
        }
        pData_storage_count = static_cast<size_t>(dataSize);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        VkResult result = s.on_vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        if (pData_storage_count != 0) {
            reply.write(pData_storage, pData_storage_count);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingShaderGroupStackSizeKHR: {
        AV_SCOPE("server:vkGetRayTracingShaderGroupStackSizeKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t group{};
        VkShaderGroupShaderKHR groupShader{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (pipeline == VK_NULL_HANDLE) return false;
        if (!r.read_pod(group)) return false;
        if (!r.read_pod(groupShader)) return false;
        VkDeviceSize result = s.on_vkGetRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader);
        reply.write_pod<uint64_t>(static_cast<uint64_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTransitionImageLayoutEXT: {
        AV_SCOPE("server:vkTransitionImageLayoutEXT");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t transitionCount{};
        VkHostImageLayoutTransitionInfo* pTransitions_storage = nullptr;
        size_t pTransitions_storage_count = 0;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(transitionCount)) return false;
        pTransitions_storage_count = static_cast<size_t>(transitionCount);
        if (pTransitions_storage_count != 0) {
            pTransitions_storage = arena.allocate<VkHostImageLayoutTransitionInfo>(pTransitions_storage_count);
            if (pTransitions_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pTransitions_storage_count; ++av_z) { pTransitions_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pTransitions_storage_count; ++av_i) {
            if (!deserialize_VkHostImageLayoutTransitionInfo(r, &pTransitions_storage[av_i], arena)) return false;
        }
        VkResult result = s.on_vkTransitionImageLayoutEXT(device, transitionCount, pTransitions_storage);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR: {
        AV_SCOPE("server:vkWriteAccelerationStructuresPropertiesKHR");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t accelerationStructureCount{};
        VkAccelerationStructureKHR* pAccelerationStructures_storage = nullptr;
        size_t pAccelerationStructures_storage_count = 0;
        VkQueryType queryType{};
        size_t dataSize{};
        uint8_t* pData_storage = nullptr;
        size_t pData_storage_count = 0;
        size_t stride{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(accelerationStructureCount)) return false;
        pAccelerationStructures_storage_count = static_cast<size_t>(accelerationStructureCount);
        if (pAccelerationStructures_storage_count != 0) {
            pAccelerationStructures_storage = arena.allocate<VkAccelerationStructureKHR>(pAccelerationStructures_storage_count);
            if (pAccelerationStructures_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pAccelerationStructures_storage_count; ++av_z) { pAccelerationStructures_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pAccelerationStructures_storage_count; ++av_i) {
            if (!deserialize_VkAccelerationStructureKHR(r, pAccelerationStructures_storage[av_i])) return false;
        }
        if (!r.read_pod(queryType)) return false;
        {
            uint64_t av_wide = 0;
            if (!r.read_pod(av_wide)) return false;
            dataSize = static_cast<size_t>(av_wide);
        }
        pData_storage_count = static_cast<size_t>(dataSize);
        if (pData_storage_count != 0) {
            pData_storage = arena.allocate<uint8_t>(pData_storage_count);
            if (pData_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        {
            uint64_t av_wide = 0;
            if (!r.read_pod(av_wide)) return false;
            stride = static_cast<size_t>(av_wide);
        }
        VkResult result = s.on_vkWriteAccelerationStructuresPropertiesKHR(device, accelerationStructureCount, pAccelerationStructures_storage, queryType, dataSize, pData_storage, stride);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pData_storage_count; ++av_z) { pData_storage[av_z] = {}; }
        }
        if (pData_storage_count != 0) {
            reply.write(pData_storage, pData_storage_count);
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT: {
        AV_SCOPE("server:vkGetCalibratedTimestampsEXT");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t timestampCount{};
        VkCalibratedTimestampInfoKHR* pTimestampInfos_storage = nullptr;
        size_t pTimestampInfos_storage_count = 0;
        uint64_t* pTimestamps_storage = nullptr;
        size_t pTimestamps_storage_count = 0;
        uint64_t pMaxDeviation_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(timestampCount)) return false;
        pTimestampInfos_storage_count = static_cast<size_t>(timestampCount);
        if (pTimestampInfos_storage_count != 0) {
            pTimestampInfos_storage = arena.allocate<VkCalibratedTimestampInfoKHR>(pTimestampInfos_storage_count);
            if (pTimestampInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pTimestampInfos_storage_count; ++av_z) { pTimestampInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pTimestampInfos_storage_count; ++av_i) {
            if (!deserialize_VkCalibratedTimestampInfoKHR(r, &pTimestampInfos_storage[av_i], arena)) return false;
        }
        pTimestamps_storage_count = static_cast<size_t>(timestampCount);
        if (pTimestamps_storage_count != 0) {
            pTimestamps_storage = arena.allocate<uint64_t>(pTimestamps_storage_count);
            if (pTimestamps_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pTimestamps_storage_count; ++av_z) { pTimestamps_storage[av_z] = {}; }
        }
        VkResult result = s.on_vkGetCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos_storage, pTimestamps_storage, &pMaxDeviation_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pTimestamps_storage_count; ++av_z) { pTimestamps_storage[av_z] = {}; }
            pMaxDeviation_out = {};
        }
        for (size_t av_k = 0; av_k < pTimestamps_storage_count; ++av_k) {
            reply.write_pod(pTimestamps_storage[av_k]);
        }
        reply.write_pod(pMaxDeviation_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT: {
        AV_SCOPE("server:vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pTimeDomainCount_local = 0;
        VkTimeDomainKHR* pTimeDomains_storage = nullptr;
        size_t pTimeDomains_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pTimeDomainCount_local)) return false;
            pTimeDomains_storage_count = static_cast<size_t>(pTimeDomainCount_local);
            if (pTimeDomains_storage_count != 0) {
                pTimeDomains_storage = arena.allocate<VkTimeDomainKHR>(pTimeDomains_storage_count);
                if (pTimeDomains_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pTimeDomains_storage_count; ++av_z) { pTimeDomains_storage[av_z] = {}; }
            }
        }
        VkResult result = s.on_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice, &pTimeDomainCount_local, array_present ? pTimeDomains_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pTimeDomainCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pTimeDomainCount_local;
        if (array_present && pTimeDomainCount_sent > pTimeDomains_storage_count) {
            pTimeDomainCount_sent = static_cast<uint32_t>(pTimeDomains_storage_count);
        }
        reply.write_pod(pTimeDomainCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pTimeDomainCount_sent; ++k) {
                reply.write_pod(pTimeDomains_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR: {
        AV_SCOPE("server:vkCmdSetRenderingAttachmentLocationsKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingAttachmentLocationInfo pLocationInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingAttachmentLocationInfo(r, &pLocationInfo_local, arena)) return false;
        s.on_vkCmdSetRenderingAttachmentLocationsKHR(commandBuffer, &pLocationInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR: {
        AV_SCOPE("server:vkCmdSetRenderingInputAttachmentIndicesKHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInputAttachmentIndexInfo pInputAttachmentIndexInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInputAttachmentIndexInfo(r, &pInputAttachmentIndexInfo_local, arena)) return false;
        s.on_vkCmdSetRenderingInputAttachmentIndicesKHR(commandBuffer, &pInputAttachmentIndexInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR: {
        AV_SCOPE("server:vkCmdBindIndexBuffer2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkDeviceSize size{};
        VkIndexType indexType{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(size)) return false;
        if (!r.read_pod(indexType)) return false;
        s.on_vkCmdBindIndexBuffer2KHR(commandBuffer, buffer, offset, size, indexType);
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineBinariesKHR: {
        AV_SCOPE("server:vkCreatePipelineBinariesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineBinaryCreateInfoKHR pCreateInfo_local{};
        VkPipelineBinaryHandlesInfoKHR pBinaries_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineBinaryCreateInfoKHR(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pBinaries_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pBinaries_out), arena)) return false;
        if (!r.read_pod(pBinaries_out.pipelineBinaryCount)) return false;
        uint8_t pBinaries_pPipelineBinaries_present = 0;
        if (!r.read_pod(pBinaries_pPipelineBinaries_present)) return false;
        if (pBinaries_pPipelineBinaries_present != 0 && pBinaries_out.pipelineBinaryCount != 0) {
            auto* pBinaries_pPipelineBinaries_storage = arena.allocate<VkPipelineBinaryKHR>(pBinaries_out.pipelineBinaryCount);
            if (pBinaries_pPipelineBinaries_storage == nullptr) return false;
            pBinaries_out.pPipelineBinaries = pBinaries_pPipelineBinaries_storage;
        }
        VkResult result = handles_room(static_cast<uint64_t>(pBinaries_out.pipelineBinaryCount)) ? s.on_vkCreatePipelineBinariesKHR(device, &pCreateInfo_local, nullptr, &pBinaries_out) : VK_ERROR_OUT_OF_HOST_MEMORY;
        handles_made_through(as_number(device));
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkPipelineBinaryHandlesInfoKHR pBinaries_blank{};
            pBinaries_blank.sType = pBinaries_out.sType;
            pBinaries_out = pBinaries_blank;
        }
        serialize_VkPipelineBinaryHandlesInfoKHR(reply, pBinaries_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR: {
        AV_SCOPE("server:vkDestroyPipelineBinaryKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineBinaryKHR pipelineBinary = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineBinaryKHR(r, pipelineBinary)) return false;
        s.on_vkDestroyPipelineBinaryKHR(device, pipelineBinary, nullptr);
        handle_ended(as_number(pipelineBinary));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR: {
        AV_SCOPE("server:vkGetDeviceImageSubresourceLayoutKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageSubresourceInfo pInfo_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageSubresourceInfo(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        s.on_vkGetDeviceImageSubresourceLayoutKHR(device, &pInfo_local, &pLayout_out);
        reply.write_pod<int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR: {
        AV_SCOPE("server:vkGetImageSubresourceLayout2KHR");
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource2 pSubresource_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (image == VK_NULL_HANDLE) return false;
        if (!deserialize_VkImageSubresource2(r, &pSubresource_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        s.on_vkGetImageSubresourceLayout2KHR(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPipelineKeyKHR: {
        AV_SCOPE("server:vkGetPipelineKeyKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCreateInfoKHR pPipelineCreateInfo_local{};
        VkPipelineBinaryKeyKHR pPipelineKey_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCreateInfoKHR(r, &pPipelineCreateInfo_local, arena)) return false;
        if (!r.read_pod(pPipelineKey_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pPipelineKey_out), arena)) return false;
        VkResult result = s.on_vkGetPipelineKeyKHR(device, &pPipelineCreateInfo_local, &pPipelineKey_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkPipelineBinaryKeyKHR pPipelineKey_blank{};
            pPipelineKey_blank.sType = pPipelineKey_out.sType;
            pPipelineKey_out = pPipelineKey_blank;
        }
        serialize_VkPipelineBinaryKeyKHR(reply, pPipelineKey_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRenderingAreaGranularityKHR: {
        AV_SCOPE("server:vkGetRenderingAreaGranularityKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkRenderingAreaInfo pRenderingAreaInfo_local{};
        VkExtent2D pGranularity_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderingAreaInfo(r, &pRenderingAreaInfo_local, arena)) return false;
        s.on_vkGetRenderingAreaGranularityKHR(device, &pRenderingAreaInfo_local, &pGranularity_out);
        reply.write_pod<int32_t>(0);
        serialize_VkExtent2D(reply, pGranularity_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkReleaseCapturedPipelineDataKHR: {
        AV_SCOPE("server:vkReleaseCapturedPipelineDataKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkReleaseCapturedPipelineDataInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkReleaseCapturedPipelineDataInfoKHR(r, &pInfo_local, arena)) return false;
        VkResult result = s.on_vkReleaseCapturedPipelineDataKHR(device, &pInfo_local, nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT: {
        AV_SCOPE("server:vkCmdBeginQueryIndexedEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        VkQueryControlFlags flags{};
        uint32_t index{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(flags)) return false;
        if (!r.read_pod(index)) return false;
        s.on_vkCmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT: {
        AV_SCOPE("server:vkCmdBeginTransformFeedbackEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstCounterBuffer{};
        uint32_t counterBufferCount{};
        VkBuffer* pCounterBuffers_storage = nullptr;
        size_t pCounterBuffers_storage_count = 0;
        VkDeviceSize* pCounterBufferOffsets_storage = nullptr;
        size_t pCounterBufferOffsets_storage_count = 0;
        uint8_t pCounterBufferOffsets_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstCounterBuffer)) return false;
        if (!r.read_pod(counterBufferCount)) return false;
        pCounterBuffers_storage_count = static_cast<size_t>(counterBufferCount);
        if (pCounterBuffers_storage_count != 0) {
            pCounterBuffers_storage = arena.allocate<VkBuffer>(pCounterBuffers_storage_count);
            if (pCounterBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCounterBuffers_storage_count; ++av_z) { pCounterBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCounterBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pCounterBuffers_storage[av_i])) return false;
        }
        if (!r.read_pod(pCounterBufferOffsets_present)) return false;
        if (pCounterBufferOffsets_present) {
            pCounterBufferOffsets_storage_count = static_cast<size_t>(counterBufferCount);
            if (pCounterBufferOffsets_storage_count != 0) {
                pCounterBufferOffsets_storage = arena.allocate<VkDeviceSize>(pCounterBufferOffsets_storage_count);
                if (pCounterBufferOffsets_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pCounterBufferOffsets_storage_count; ++av_z) { pCounterBufferOffsets_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pCounterBufferOffsets_storage_count; ++av_i) {
                if (!r.read_pod(pCounterBufferOffsets_storage[av_i])) return false;
            }
        }
        s.on_vkCmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers_storage, pCounterBufferOffsets_present ? pCounterBufferOffsets_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT: {
        AV_SCOPE("server:vkCmdBindTransformFeedbackBuffersEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        VkBuffer* pBuffers_storage = nullptr;
        size_t pBuffers_storage_count = 0;
        VkDeviceSize* pOffsets_storage = nullptr;
        size_t pOffsets_storage_count = 0;
        VkDeviceSize* pSizes_storage = nullptr;
        size_t pSizes_storage_count = 0;
        uint8_t pSizes_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage_count = static_cast<size_t>(bindingCount);
        if (pBuffers_storage_count != 0) {
            pBuffers_storage = arena.allocate<VkBuffer>(pBuffers_storage_count);
            if (pBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pBuffers_storage_count; ++av_z) { pBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[av_i])) return false;
        }
        pOffsets_storage_count = static_cast<size_t>(bindingCount);
        if (pOffsets_storage_count != 0) {
            pOffsets_storage = arena.allocate<VkDeviceSize>(pOffsets_storage_count);
            if (pOffsets_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pOffsets_storage_count; ++av_z) { pOffsets_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pOffsets_storage_count; ++av_i) {
            if (!r.read_pod(pOffsets_storage[av_i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage_count = static_cast<size_t>(bindingCount);
            if (pSizes_storage_count != 0) {
                pSizes_storage = arena.allocate<VkDeviceSize>(pSizes_storage_count);
                if (pSizes_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pSizes_storage_count; ++av_z) { pSizes_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pSizes_storage_count; ++av_i) {
                if (!r.read_pod(pSizes_storage[av_i])) return false;
            }
        }
        s.on_vkCmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers_storage, pOffsets_storage, pSizes_present ? pSizes_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT: {
        AV_SCOPE("server:vkCmdDrawIndirectByteCountEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t instanceCount{};
        uint32_t firstInstance{};
        VkBuffer counterBuffer = VK_NULL_HANDLE;
        VkDeviceSize counterBufferOffset{};
        uint32_t counterOffset{};
        uint32_t vertexStride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(instanceCount)) return false;
        if (!r.read_pod(firstInstance)) return false;
        if (!deserialize_VkBuffer(r, counterBuffer)) return false;
        if (counterBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(counterBufferOffset)) return false;
        if (!r.read_pod(counterOffset)) return false;
        if (!r.read_pod(vertexStride)) return false;
        s.on_vkCmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT: {
        AV_SCOPE("server:vkCmdEndQueryIndexedEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        uint32_t index{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (queryPool == VK_NULL_HANDLE) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(index)) return false;
        s.on_vkCmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT: {
        AV_SCOPE("server:vkCmdEndTransformFeedbackEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstCounterBuffer{};
        uint32_t counterBufferCount{};
        VkBuffer* pCounterBuffers_storage = nullptr;
        size_t pCounterBuffers_storage_count = 0;
        VkDeviceSize* pCounterBufferOffsets_storage = nullptr;
        size_t pCounterBufferOffsets_storage_count = 0;
        uint8_t pCounterBufferOffsets_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstCounterBuffer)) return false;
        if (!r.read_pod(counterBufferCount)) return false;
        pCounterBuffers_storage_count = static_cast<size_t>(counterBufferCount);
        if (pCounterBuffers_storage_count != 0) {
            pCounterBuffers_storage = arena.allocate<VkBuffer>(pCounterBuffers_storage_count);
            if (pCounterBuffers_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pCounterBuffers_storage_count; ++av_z) { pCounterBuffers_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pCounterBuffers_storage_count; ++av_i) {
            if (!deserialize_VkBuffer(r, pCounterBuffers_storage[av_i])) return false;
        }
        if (!r.read_pod(pCounterBufferOffsets_present)) return false;
        if (pCounterBufferOffsets_present) {
            pCounterBufferOffsets_storage_count = static_cast<size_t>(counterBufferCount);
            if (pCounterBufferOffsets_storage_count != 0) {
                pCounterBufferOffsets_storage = arena.allocate<VkDeviceSize>(pCounterBufferOffsets_storage_count);
                if (pCounterBufferOffsets_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pCounterBufferOffsets_storage_count; ++av_z) { pCounterBufferOffsets_storage[av_z] = {}; }
            }
            for (size_t av_i = 0; av_i < pCounterBufferOffsets_storage_count; ++av_i) {
                if (!r.read_pod(pCounterBufferOffsets_storage[av_i])) return false;
            }
        }
        s.on_vkCmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers_storage, pCounterBufferOffsets_present ? pCounterBufferOffsets_storage : nullptr);
        return true;
    }
    case ::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT: {
        AV_SCOPE("server:vkSetDeviceMemoryPriorityEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        float priority{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (memory == VK_NULL_HANDLE) return false;
        if (!r.read_pod(priority)) return false;
        s.on_vkSetDeviceMemoryPriorityEXT(device, memory, priority);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetFenceFdKHR: {
        AV_SCOPE("server:vkGetFenceFdKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkFenceGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFenceGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        VkResult result = s.on_vkGetFenceFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pFd_out = {};
        }
        reply.write_pod(pFd_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetMemoryFdKHR: {
        AV_SCOPE("server:vkGetMemoryFdKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkMemoryGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkMemoryGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        VkResult result = s.on_vkGetMemoryFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pFd_out = {};
        }
        reply.write_pod(pFd_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR: {
        AV_SCOPE("server:vkGetMemoryFdPropertiesKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkExternalMemoryHandleTypeFlagBits handleType{};
        int fd{};
        VkMemoryFdPropertiesKHR pMemoryFdProperties_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(handleType)) return false;
        if (!r.read_pod(fd)) return false;
        if (!r.read_pod(pMemoryFdProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryFdProperties_out), arena)) return false;
        VkResult result = s.on_vkGetMemoryFdPropertiesKHR(device, handleType, fd, &pMemoryFdProperties_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            VkMemoryFdPropertiesKHR pMemoryFdProperties_blank{};
            pMemoryFdProperties_blank.sType = pMemoryFdProperties_out.sType;
            pMemoryFdProperties_out = pMemoryFdProperties_blank;
        }
        serialize_VkMemoryFdPropertiesKHR(reply, pMemoryFdProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreFdKHR: {
        AV_SCOPE("server:vkGetSemaphoreFdKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        VkResult result = s.on_vkGetSemaphoreFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            pFd_out = {};
        }
        reply.write_pod(pFd_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkImportFenceFdKHR: {
        AV_SCOPE("server:vkImportFenceFdKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkImportFenceFdInfoKHR pImportFenceFdInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImportFenceFdInfoKHR(r, &pImportFenceFdInfo_local, arena)) return false;
        VkResult result = s.on_vkImportFenceFdKHR(device, &pImportFenceFdInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkImportSemaphoreFdKHR: {
        AV_SCOPE("server:vkImportSemaphoreFdKHR");
        VkDevice device = VK_NULL_HANDLE;
        VkImportSemaphoreFdInfoKHR pImportSemaphoreFdInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImportSemaphoreFdInfoKHR(r, &pImportSemaphoreFdInfo_local, arena)) return false;
        VkResult result = s.on_vkImportSemaphoreFdKHR(device, &pImportSemaphoreFdInfo_local);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkAntiLagUpdateAMD: {
        AV_SCOPE("server:vkAntiLagUpdateAMD");
        VkDevice device = VK_NULL_HANDLE;
        VkAntiLagDataAMD pData_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAntiLagDataAMD(r, &pData_local, arena)) return false;
        s.on_vkAntiLagUpdateAMD(device, &pData_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT: {
        AV_SCOPE("server:vkCmdBindDescriptorBufferEmbeddedSamplers2EXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBindDescriptorBufferEmbeddedSamplersInfoEXT pBindDescriptorBufferEmbeddedSamplersInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBindDescriptorBufferEmbeddedSamplersInfoEXT(r, &pBindDescriptorBufferEmbeddedSamplersInfo_local, arena)) return false;
        s.on_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(commandBuffer, &pBindDescriptorBufferEmbeddedSamplersInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindDescriptorSets2KHR: {
        AV_SCOPE("server:vkCmdBindDescriptorSets2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBindDescriptorSetsInfo pBindDescriptorSetsInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBindDescriptorSetsInfo(r, &pBindDescriptorSetsInfo_local, arena)) return false;
        s.on_vkCmdBindDescriptorSets2KHR(commandBuffer, &pBindDescriptorSetsInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMultiEXT: {
        AV_SCOPE("server:vkCmdDrawMultiEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t drawCount{};
        VkMultiDrawInfoEXT* pVertexInfo_storage = nullptr;
        size_t pVertexInfo_storage_count = 0;
        uint32_t instanceCount{};
        uint32_t firstInstance{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(drawCount)) return false;
        pVertexInfo_storage_count = static_cast<size_t>(drawCount);
        if (pVertexInfo_storage_count != 0) {
            pVertexInfo_storage = arena.allocate<VkMultiDrawInfoEXT>(pVertexInfo_storage_count);
            if (pVertexInfo_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pVertexInfo_storage_count; ++av_z) { pVertexInfo_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pVertexInfo_storage_count; ++av_i) {
            if (!deserialize_VkMultiDrawInfoEXT(r, &pVertexInfo_storage[av_i], arena)) return false;
        }
        if (!r.read_pod(instanceCount)) return false;
        if (!r.read_pod(firstInstance)) return false;
        if (!r.read_pod(stride)) return false;
        s.on_vkCmdDrawMultiEXT(commandBuffer, drawCount, pVertexInfo_storage, instanceCount, firstInstance, stride);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushConstants2KHR: {
        AV_SCOPE("server:vkCmdPushConstants2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPushConstantsInfo pPushConstantsInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkPushConstantsInfo(r, &pPushConstantsInfo_local, arena)) return false;
        s.on_vkCmdPushConstants2KHR(commandBuffer, &pPushConstantsInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushDescriptorSet2KHR: {
        AV_SCOPE("server:vkCmdPushDescriptorSet2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPushDescriptorSetInfo pPushDescriptorSetInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkPushDescriptorSetInfo(r, &pPushDescriptorSetInfo_local, arena)) return false;
        s.on_vkCmdPushDescriptorSet2KHR(commandBuffer, &pPushDescriptorSetInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushDescriptorSetWithTemplate2KHR: {
        AV_SCOPE("server:vkCmdPushDescriptorSetWithTemplate2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPushDescriptorSetWithTemplateInfo pPushDescriptorSetWithTemplateInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkPushDescriptorSetWithTemplateInfo(r, &pPushDescriptorSetWithTemplateInfo_local, arena)) return false;
        s.on_vkCmdPushDescriptorSetWithTemplate2KHR(commandBuffer, &pPushDescriptorSetWithTemplateInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBias2EXT: {
        AV_SCOPE("server:vkCmdSetDepthBias2EXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDepthBiasInfoEXT pDepthBiasInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDepthBiasInfoEXT(r, &pDepthBiasInfo_local, arena)) return false;
        s.on_vkCmdSetDepthBias2EXT(commandBuffer, &pDepthBiasInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDescriptorBufferOffsets2EXT: {
        AV_SCOPE("server:vkCmdSetDescriptorBufferOffsets2EXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSetDescriptorBufferOffsetsInfoEXT pSetDescriptorBufferOffsetsInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSetDescriptorBufferOffsetsInfoEXT(r, &pSetDescriptorBufferOffsetsInfo_local, arena)) return false;
        s.on_vkCmdSetDescriptorBufferOffsets2EXT(commandBuffer, &pSetDescriptorBufferOffsetsInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetSampleLocationsEXT: {
        AV_SCOPE("server:vkCmdSetSampleLocationsEXT");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSampleLocationsInfoEXT pSampleLocationsInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSampleLocationsInfoEXT(r, &pSampleLocationsInfo_local, arena)) return false;
        s.on_vkCmdSetSampleLocationsEXT(commandBuffer, &pSampleLocationsInfo_local);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdTraceRaysIndirect2KHR: {
        AV_SCOPE("server:vkCmdTraceRaysIndirect2KHR");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDeviceAddress indirectDeviceAddress{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(indirectDeviceAddress)) return false;
        s.on_vkCmdTraceRaysIndirect2KHR(commandBuffer, indirectDeviceAddress);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteBufferMarker2AMD: {
        AV_SCOPE("server:vkCmdWriteBufferMarker2AMD");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stage{};
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        uint32_t marker{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stage)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(marker)) return false;
        s.on_vkCmdWriteBufferMarker2AMD(commandBuffer, stage, dstBuffer, dstOffset, marker);
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteBufferMarkerAMD: {
        AV_SCOPE("server:vkCmdWriteBufferMarkerAMD");
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlagBits pipelineStage{};
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        uint32_t marker{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineStage)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (dstBuffer == VK_NULL_HANDLE) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(marker)) return false;
        s.on_vkCmdWriteBufferMarkerAMD(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetCalibratedTimestampsKHR: {
        AV_SCOPE("server:vkGetCalibratedTimestampsKHR");
        VkDevice device = VK_NULL_HANDLE;
        uint32_t timestampCount{};
        VkCalibratedTimestampInfoKHR* pTimestampInfos_storage = nullptr;
        size_t pTimestampInfos_storage_count = 0;
        uint64_t* pTimestamps_storage = nullptr;
        size_t pTimestamps_storage_count = 0;
        uint64_t pMaxDeviation_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(timestampCount)) return false;
        pTimestampInfos_storage_count = static_cast<size_t>(timestampCount);
        if (pTimestampInfos_storage_count != 0) {
            pTimestampInfos_storage = arena.allocate<VkCalibratedTimestampInfoKHR>(pTimestampInfos_storage_count);
            if (pTimestampInfos_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pTimestampInfos_storage_count; ++av_z) { pTimestampInfos_storage[av_z] = {}; }
        }
        for (size_t av_i = 0; av_i < pTimestampInfos_storage_count; ++av_i) {
            if (!deserialize_VkCalibratedTimestampInfoKHR(r, &pTimestampInfos_storage[av_i], arena)) return false;
        }
        pTimestamps_storage_count = static_cast<size_t>(timestampCount);
        if (pTimestamps_storage_count != 0) {
            pTimestamps_storage = arena.allocate<uint64_t>(pTimestamps_storage_count);
            if (pTimestamps_storage == nullptr) return false;
            for (size_t av_z = 0; av_z < pTimestamps_storage_count; ++av_z) { pTimestamps_storage[av_z] = {}; }
        }
        VkResult result = s.on_vkGetCalibratedTimestampsKHR(device, timestampCount, pTimestampInfos_storage, pTimestamps_storage, &pMaxDeviation_out);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        if (static_cast<int32_t>(result) < 0) {
            for (size_t av_z = 0; av_z < pTimestamps_storage_count; ++av_z) { pTimestamps_storage[av_z] = {}; }
            pMaxDeviation_out = {};
        }
        for (size_t av_k = 0; av_k < pTimestamps_storage_count; ++av_k) {
            reply.write_pod(pTimestamps_storage[av_k]);
        }
        reply.write_pod(pMaxDeviation_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceCalibrateableTimeDomainsKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pTimeDomainCount_local = 0;
        VkTimeDomainKHR* pTimeDomains_storage = nullptr;
        size_t pTimeDomains_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pTimeDomainCount_local)) return false;
            pTimeDomains_storage_count = static_cast<size_t>(pTimeDomainCount_local);
            if (pTimeDomains_storage_count != 0) {
                pTimeDomains_storage = arena.allocate<VkTimeDomainKHR>(pTimeDomains_storage_count);
                if (pTimeDomains_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pTimeDomains_storage_count; ++av_z) { pTimeDomains_storage[av_z] = {}; }
            }
        }
        VkResult result = s.on_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(physicalDevice, &pTimeDomainCount_local, array_present ? pTimeDomains_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pTimeDomainCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pTimeDomainCount_local;
        if (array_present && pTimeDomainCount_sent > pTimeDomains_storage_count) {
            pTimeDomainCount_sent = static_cast<uint32_t>(pTimeDomains_storage_count);
        }
        reply.write_pod(pTimeDomainCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pTimeDomainCount_sent; ++k) {
                reply.write_pod(pTimeDomains_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR: {
        AV_SCOPE("server:vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint8_t array_present = 0;
        uint32_t pPropertyCount_local = 0;
        VkCooperativeMatrixPropertiesKHR* pProperties_storage = nullptr;
        size_t pProperties_storage_count = 0;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage_count = static_cast<size_t>(pPropertyCount_local);
            if (pProperties_storage_count != 0) {
                pProperties_storage = arena.allocate<VkCooperativeMatrixPropertiesKHR>(pProperties_storage_count);
                if (pProperties_storage == nullptr) return false;
                for (size_t av_z = 0; av_z < pProperties_storage_count; ++av_z) { pProperties_storage[av_z] = {}; }
            }
            for (size_t av_e = 0; av_e < pProperties_storage_count; ++av_e) { pProperties_storage[av_e].sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR; pProperties_storage[av_e].pNext = nullptr; }
        }
        VkResult result = s.on_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, &pPropertyCount_local, array_present ? pProperties_storage : nullptr);
        reply.write_pod<int32_t>(static_cast<int32_t>(result));
        uint32_t pPropertyCount_sent = static_cast<int32_t>(result) < 0 ? 0u : pPropertyCount_local;
        if (array_present && pPropertyCount_sent > pProperties_storage_count) {
            pPropertyCount_sent = static_cast<uint32_t>(pProperties_storage_count);
        }
        reply.write_pod(pPropertyCount_sent);
        if (array_present) {
            for (uint32_t k = 0; k < pPropertyCount_sent; ++k) {
                serialize_VkCooperativeMatrixPropertiesKHR(reply, pProperties_storage[k]);
            }
        }
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMultisamplePropertiesEXT: {
        AV_SCOPE("server:vkGetPhysicalDeviceMultisamplePropertiesEXT");
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkSampleCountFlagBits samples{};
        VkMultisamplePropertiesEXT pMultisampleProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(samples)) return false;
        if (!r.read_pod(pMultisampleProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMultisampleProperties_out), arena)) return false;
        s.on_vkGetPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, &pMultisampleProperties_out);
        reply.write_pod<int32_t>(0);
        serialize_VkMultisamplePropertiesEXT(reply, pMultisampleProperties_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetShaderModuleCreateInfoIdentifierEXT: {
        AV_SCOPE("server:vkGetShaderModuleCreateInfoIdentifierEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo pCreateInfo_local{};
        VkShaderModuleIdentifierEXT pIdentifier_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModuleCreateInfo(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pIdentifier_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pIdentifier_out), arena)) return false;
        s.on_vkGetShaderModuleCreateInfoIdentifierEXT(device, &pCreateInfo_local, &pIdentifier_out);
        reply.write_pod<int32_t>(0);
        serialize_VkShaderModuleIdentifierEXT(reply, pIdentifier_out);
        return true;
    }
    case ::av::vkproto::kCmd_vkGetShaderModuleIdentifierEXT: {
        AV_SCOPE("server:vkGetShaderModuleIdentifierEXT");
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkShaderModuleIdentifierEXT pIdentifier_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModule(r, shaderModule)) return false;
        if (shaderModule == VK_NULL_HANDLE) return false;
        if (!r.read_pod(pIdentifier_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pIdentifier_out), arena)) return false;
        s.on_vkGetShaderModuleIdentifierEXT(device, shaderModule, &pIdentifier_out);
        reply.write_pod<int32_t>(0);
        serialize_VkShaderModuleIdentifierEXT(reply, pIdentifier_out);
        return true;
    }
    default:
        return false;
    }
}

}  // namespace av::vkproto
