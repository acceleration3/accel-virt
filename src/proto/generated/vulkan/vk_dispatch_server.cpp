// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m rvk_codegen --xml ... --out generated/)


#include "vk_dispatch_server.h"

#include <vector>

#include "rvk/instr/log.hpp"
#include "vk_protocol_ids.h"
#include "vk_serialize.h"

namespace av::vkproto {

bool dispatch_command(ServerDispatch& s, std::uint32_t cmd_id, ::av::WireReader& r, ::av::WireWriter& reply) {
    switch (cmd_id) {
    case ::av::vkproto::kCmd_vkDestroyInstance: {
        ::av::WireArena arena;
        (void)arena;
        VkInstance instance = VK_NULL_HANDLE;
        if (!deserialize_VkInstance(r, instance)) return false;
        RVK_TRACE("server vkDestroyInstance(instance={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(instance));
        s.on_vkDestroyInstance(instance, nullptr);
        RVK_TRACE("server vkDestroyInstance -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDevice: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        RVK_TRACE("server vkDestroyDevice(device={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkDestroyDevice(device, nullptr);
        RVK_TRACE("server vkDestroyDevice -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceQueue: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex{};
        uint32_t queueIndex{};
        VkQueue pQueue_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(queueFamilyIndex)) return false;
        if (!r.read_pod(queueIndex)) return false;
        RVK_TRACE("server vkGetDeviceQueue(device={:#x}, queueFamilyIndex={}, queueIndex={}, pQueue=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(queueFamilyIndex), static_cast<long long>(queueIndex));
        s.on_vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &pQueue_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkQueue(reply, pQueue_out);
        RVK_TRACE("server vkGetDeviceQueue -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDevices: {
        ::av::WireArena arena;
        (void)arena;
        VkInstance instance = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pPhysicalDeviceCount_local = 0;
        std::vector<VkPhysicalDevice> pPhysicalDevices_storage;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceCount_local)) return false;
            pPhysicalDevices_storage.resize(pPhysicalDeviceCount_local);
        }
        RVK_TRACE("server vkEnumeratePhysicalDevices(instance={:#x}, pPhysicalDeviceCount=ptr, pPhysicalDevices=ptr)",::rvk::instr::trace_handle(instance));
        VkResult result = s.on_vkEnumeratePhysicalDevices(instance, &pPhysicalDeviceCount_local, array_present ? pPhysicalDevices_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pPhysicalDeviceCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPhysicalDeviceCount_local; ++k) {
                serialize_VkPhysicalDevice(reply, pPhysicalDevices_storage[k]);
            }
        }
        RVK_TRACE("server vkEnumeratePhysicalDevices -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceProperties2(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceProperties2(physicalDevice, &pProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceProperties2(reply, pProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceProperties2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateInstance: {
        ::av::WireArena arena;
        (void)arena;
        VkInstanceCreateInfo pCreateInfo_local{};
        VkInstance pInstance_out = VK_NULL_HANDLE;
        if (!deserialize_VkInstanceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateInstance(pCreateInfo=ptr, pAllocator=ptr, pInstance=ptr)");
        VkResult result = s.on_vkCreateInstance(&pCreateInfo_local, nullptr, &pInstance_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkInstance(reply, pInstance_out);
        RVK_TRACE("server vkCreateInstance -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDevice: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDeviceCreateInfo pCreateInfo_local{};
        VkDevice pDevice_out = VK_NULL_HANDLE;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkDeviceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateDevice(physicalDevice={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDevice=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkCreateDevice(physicalDevice, &pCreateInfo_local, nullptr, &pDevice_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDevice(reply, pDevice_out);
        RVK_TRACE("server vkCreateDevice -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceProperties(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceProperties(physicalDevice, &pProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceProperties(reply, pProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFeatures(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceFeatures(physicalDevice, &pFeatures_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceFeatures(reply, pFeatures_out);
        RVK_TRACE("server vkGetPhysicalDeviceFeatures -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
        s.on_vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkFormatProperties(reply, pFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties(physicalDevice={:#x}, format={}, type={}, tiling={}, usage={}, flags={}, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format), static_cast<long long>(type), static_cast<long long>(tiling), static_cast<long long>(usage), static_cast<long long>(flags));
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, &pImageFormatProperties_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkImageFormatProperties(reply, pImageFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceMemoryProperties(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties(reply, pMemoryProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pQueueFamilyPropertyCount_local = 0;
        std::vector<VkQueueFamilyProperties> pQueueFamilyProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage.resize(pQueueFamilyPropertyCount_local);
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pQueueFamilyPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pQueueFamilyPropertyCount_local; ++k) {
                serialize_VkQueueFamilyProperties(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkImageType type{};
        VkSampleCountFlagBits samples{};
        VkImageUsageFlags usage{};
        VkImageTiling tiling{};
        std::uint8_t array_present = 0;
        std::uint32_t pPropertyCount_local = 0;
        std::vector<VkSparseImageFormatProperties> pProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(type)) return false;
        if (!r.read_pod(samples)) return false;
        if (!r.read_pod(usage)) return false;
        if (!r.read_pod(tiling)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage.resize(pPropertyCount_local);
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice={:#x}, format={}, type={}, samples={}, usage={}, tiling={}, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format), static_cast<long long>(type), static_cast<long long>(samples), static_cast<long long>(usage), static_cast<long long>(tiling));
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, &pPropertyCount_local, array_present ? pProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPropertyCount_local; ++k) {
                serialize_VkSparseImageFormatProperties(reply, pProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumerateDeviceExtensionProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        const char* pLayerName = nullptr;
        std::uint8_t array_present = 0;
        std::uint32_t pPropertyCount_local = 0;
        std::vector<VkExtensionProperties> pProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        {
            std::uint64_t n = 0;
            if (!r.read_varint(n)) return false;
            if (n > 0) {
                char* tmp_str = arena.allocate_chars(static_cast<std::size_t>(n));
                if (tmp_str == nullptr) { return false; }
                if (!r.read(tmp_str, static_cast<std::size_t>(n))) return false;
                pLayerName = tmp_str;
            }
        }
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage.resize(pPropertyCount_local);
        }
        RVK_TRACE("server vkEnumerateDeviceExtensionProperties(physicalDevice={:#x}, pLayerName=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, &pPropertyCount_local, array_present ? pProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPropertyCount_local; ++k) {
                serialize_VkExtensionProperties(reply, pProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkEnumerateDeviceExtensionProperties -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumerateDeviceLayerProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pPropertyCount_local = 0;
        std::vector<VkLayerProperties> pProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage.resize(pPropertyCount_local);
        }
        RVK_TRACE("server vkEnumerateDeviceLayerProperties(physicalDevice={:#x}, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkEnumerateDeviceLayerProperties(physicalDevice, &pPropertyCount_local, array_present ? pProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPropertyCount_local; ++k) {
                serialize_VkLayerProperties(reply, pProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkEnumerateDeviceLayerProperties -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures2 pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pFeatures_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFeatures_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFeatures2(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceFeatures2(physicalDevice, &pFeatures_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceFeatures2(reply, pFeatures_out);
        RVK_TRACE("server vkGetPhysicalDeviceFeatures2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties2 pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(pFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFormatProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties2(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
        s.on_vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkFormatProperties2(reply, pFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceImageFormatInfo2 pImageFormatInfo_local{};
        VkImageFormatProperties2 pImageFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceImageFormatInfo2(r, &pImageFormatInfo_local, arena)) return false;
        if (!r.read_pod(pImageFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pImageFormatProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties2(physicalDevice={:#x}, pImageFormatInfo=ptr, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &pImageFormatInfo_local, &pImageFormatProperties_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkImageFormatProperties2(reply, pImageFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties2 -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties2 pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pMemoryProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties2(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties2(reply, pMemoryProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pQueueFamilyPropertyCount_local = 0;
        std::vector<VkQueueFamilyProperties2> pQueueFamilyProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage.resize(pQueueFamilyPropertyCount_local);
            for (auto& __e : pQueueFamilyProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pQueueFamilyPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pQueueFamilyPropertyCount_local; ++k) {
                serialize_VkQueueFamilyProperties2(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceSparseImageFormatInfo2 pFormatInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pPropertyCount_local = 0;
        std::vector<VkSparseImageFormatProperties2> pProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceSparseImageFormatInfo2(r, &pFormatInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage.resize(pPropertyCount_local);
            for (auto& __e : pProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice={:#x}, pFormatInfo=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &pFormatInfo_local, &pPropertyCount_local, array_present ? pProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPropertyCount_local; ++k) {
                serialize_VkSparseImageFormatProperties2(reply, pProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDeviceWaitIdle: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        RVK_TRACE("server vkDeviceWaitIdle(device={:#x})",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkDeviceWaitIdle(device);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkDeviceWaitIdle -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueWaitIdle: {
        ::av::WireArena arena;
        (void)arena;
        VkQueue queue = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        RVK_TRACE("server vkQueueWaitIdle(queue={:#x})",::rvk::instr::trace_handle(queue));
        VkResult result = s.on_vkQueueWaitIdle(queue);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkQueueWaitIdle -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateMemory: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkMemoryAllocateInfo pAllocateInfo_local{};
        VkDeviceMemory pMemory_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkMemoryAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        RVK_TRACE("server vkAllocateMemory(device={:#x}, pAllocateInfo=ptr, pAllocator=ptr, pMemory=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkAllocateMemory(device, &pAllocateInfo_local, nullptr, &pMemory_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDeviceMemory(reply, pMemory_out);
        RVK_TRACE("server vkAllocateMemory -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeMemory: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        RVK_TRACE("server vkFreeMemory(device={:#x}, memory={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory));
        s.on_vkFreeMemory(device, memory, nullptr);
        RVK_TRACE("server vkFreeMemory -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize memoryOffset{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (!r.read_pod(memoryOffset)) return false;
        RVK_TRACE("server vkBindBufferMemory(device={:#x}, buffer={:#x}, memory={:#x}, memoryOffset={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer), ::rvk::instr::trace_handle(memory), static_cast<long long>(memoryOffset));
        VkResult result = s.on_vkBindBufferMemory(device, buffer, memory, memoryOffset);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindBufferMemory -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize memoryOffset{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (!r.read_pod(memoryOffset)) return false;
        RVK_TRACE("server vkBindImageMemory(device={:#x}, image={:#x}, memory={:#x}, memoryOffset={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image), ::rvk::instr::trace_handle(memory), static_cast<long long>(memoryOffset));
        VkResult result = s.on_vkBindImageMemory(device, image, memory, memoryOffset);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindImageMemory -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkMemoryRequirements pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        RVK_TRACE("server vkGetBufferMemoryRequirements(device={:#x}, buffer={:#x}, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer));
        s.on_vkGetBufferMemoryRequirements(device, buffer, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetBufferMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkMemoryRequirements pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        RVK_TRACE("server vkGetImageMemoryRequirements(device={:#x}, image={:#x}, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkGetImageMemoryRequirements(device, image, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetImageMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferCreateInfo pCreateInfo_local{};
        VkBuffer pBuffer_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateBuffer(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pBuffer=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateBuffer(device, &pCreateInfo_local, nullptr, &pBuffer_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkBuffer(reply, pBuffer_out);
        RVK_TRACE("server vkCreateBuffer -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        RVK_TRACE("server vkDestroyBuffer(device={:#x}, buffer={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(buffer));
        s.on_vkDestroyBuffer(device, buffer, nullptr);
        RVK_TRACE("server vkDestroyBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateImage: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageCreateInfo pCreateInfo_local{};
        VkImage pImage_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateImage(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pImage=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateImage(device, &pCreateInfo_local, nullptr, &pImage_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkImage(reply, pImage_out);
        RVK_TRACE("server vkCreateImage -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyImage: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        RVK_TRACE("server vkDestroyImage(device={:#x}, image={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkDestroyImage(device, image, nullptr);
        RVK_TRACE("server vkDestroyImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateImageView: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageViewCreateInfo pCreateInfo_local{};
        VkImageView pView_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageViewCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateImageView(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pView=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateImageView(device, &pCreateInfo_local, nullptr, &pView_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkImageView(reply, pView_out);
        RVK_TRACE("server vkCreateImageView -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyImageView: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageView(r, imageView)) return false;
        RVK_TRACE("server vkDestroyImageView(device={:#x}, imageView={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(imageView));
        s.on_vkDestroyImageView(device, imageView, nullptr);
        RVK_TRACE("server vkDestroyImageView -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSampler: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerCreateInfo pCreateInfo_local{};
        VkSampler pSampler_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateSampler(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSampler=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateSampler(device, &pCreateInfo_local, nullptr, &pSampler_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkSampler(reply, pSampler_out);
        RVK_TRACE("server vkCreateSampler -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySampler: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSampler(r, sampler)) return false;
        RVK_TRACE("server vkDestroySampler(device={:#x}, sampler={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(sampler));
        s.on_vkDestroySampler(device, sampler, nullptr);
        RVK_TRACE("server vkDestroySampler -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSemaphore: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo pCreateInfo_local{};
        VkSemaphore pSemaphore_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateSemaphore(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSemaphore=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateSemaphore(device, &pCreateInfo_local, nullptr, &pSemaphore_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkSemaphore(reply, pSemaphore_out);
        RVK_TRACE("server vkCreateSemaphore -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySemaphore: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        RVK_TRACE("server vkDestroySemaphore(device={:#x}, semaphore={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
        s.on_vkDestroySemaphore(device, semaphore, nullptr);
        RVK_TRACE("server vkDestroySemaphore -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateFence: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFenceCreateInfo pCreateInfo_local{};
        VkFence pFence_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFenceCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateFence(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pFence=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateFence(device, &pCreateInfo_local, nullptr, &pFence_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkFence(reply, pFence_out);
        RVK_TRACE("server vkCreateFence -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyFence: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkDestroyFence(device={:#x}, fence={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(fence));
        s.on_vkDestroyFence(device, fence, nullptr);
        RVK_TRACE("server vkDestroyFence -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkResetFences: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t fenceCount{};
        std::vector<VkFence> pFences_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(fenceCount)) return false;
        pFences_storage.resize(static_cast<std::size_t>(fenceCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(fenceCount); ++__i) {
            if (!deserialize_VkFence(r, pFences_storage[__i])) return false;
        }
        RVK_TRACE("server vkResetFences(device={:#x}, fenceCount={}, pFences=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(fenceCount));
        VkResult result = s.on_vkResetFences(device, fenceCount, pFences_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkResetFences -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitForFences: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t fenceCount{};
        std::vector<VkFence> pFences_storage;
        VkBool32 waitAll{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(fenceCount)) return false;
        pFences_storage.resize(static_cast<std::size_t>(fenceCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(fenceCount); ++__i) {
            if (!deserialize_VkFence(r, pFences_storage[__i])) return false;
        }
        if (!r.read_pod(waitAll)) return false;
        if (!r.read_pod(timeout)) return false;
        RVK_TRACE("server vkWaitForFences(device={:#x}, fenceCount={}, pFences=ptr, waitAll={}, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(fenceCount), static_cast<long long>(waitAll), static_cast<long long>(timeout));
        VkResult result = s.on_vkWaitForFences(device, fenceCount, pFences_storage.data(), waitAll, timeout);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkWaitForFences -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetFenceStatus: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkGetFenceStatus(device={:#x}, fence={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(fence));
        VkResult result = s.on_vkGetFenceStatus(device, fence);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkGetFenceStatus -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateCommandPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pCreateInfo_local{};
        VkCommandPool pCommandPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateCommandPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pCommandPool=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateCommandPool(device, &pCreateInfo_local, nullptr, &pCommandPool_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkCommandPool(reply, pCommandPool_out);
        RVK_TRACE("server vkCreateCommandPool -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyCommandPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        RVK_TRACE("server vkDestroyCommandPool(device={:#x}, commandPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool));
        s.on_vkDestroyCommandPool(device, commandPool, nullptr);
        RVK_TRACE("server vkDestroyCommandPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkResetCommandPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolResetFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkResetCommandPool(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
        VkResult result = s.on_vkResetCommandPool(device, commandPool, flags);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkResetCommandPool -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBeginCommandBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferBeginInfo pBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCommandBufferBeginInfo(r, &pBeginInfo_local, arena)) return false;
        RVK_TRACE("server vkBeginCommandBuffer(commandBuffer={:#x}, pBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        VkResult result = s.on_vkBeginCommandBuffer(commandBuffer, &pBeginInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBeginCommandBuffer -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkEndCommandBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        RVK_TRACE("server vkEndCommandBuffer(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
        VkResult result = s.on_vkEndCommandBuffer(commandBuffer);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkEndCommandBuffer -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetCommandBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferResetFlags flags{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkResetCommandBuffer(commandBuffer={:#x}, flags={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(flags));
        VkResult result = s.on_vkResetCommandBuffer(commandBuffer, flags);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkResetCommandBuffer -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyShaderModule: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModule(r, shaderModule)) return false;
        RVK_TRACE("server vkDestroyShaderModule(device={:#x}, shaderModule={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(shaderModule));
        s.on_vkDestroyShaderModule(device, shaderModule, nullptr);
        RVK_TRACE("server vkDestroyShaderModule -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateShaderModule: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo pCreateInfo_local{};
        VkShaderModule pShaderModule_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderModuleCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateShaderModule(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pShaderModule=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateShaderModule(device, &pCreateInfo_local, nullptr, &pShaderModule_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkShaderModule(reply, pShaderModule_out);
        RVK_TRACE("server vkCreateShaderModule -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineLayout: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo pCreateInfo_local{};
        VkPipelineLayout pPipelineLayout_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreatePipelineLayout(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPipelineLayout=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreatePipelineLayout(device, &pCreateInfo_local, nullptr, &pPipelineLayout_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkPipelineLayout(reply, pPipelineLayout_out);
        RVK_TRACE("server vkCreatePipelineLayout -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineLayout: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineLayout(r, pipelineLayout)) return false;
        RVK_TRACE("server vkDestroyPipelineLayout(device={:#x}, pipelineLayout={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineLayout));
        s.on_vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        RVK_TRACE("server vkDestroyPipelineLayout -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipeline: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        RVK_TRACE("server vkDestroyPipeline(device={:#x}, pipeline={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline));
        s.on_vkDestroyPipeline(device, pipeline, nullptr);
        RVK_TRACE("server vkDestroyPipeline -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineCache: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCacheCreateInfo pCreateInfo_local{};
        VkPipelineCache pPipelineCache_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCacheCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreatePipelineCache(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPipelineCache=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreatePipelineCache(device, &pCreateInfo_local, nullptr, &pPipelineCache_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkPipelineCache(reply, pPipelineCache_out);
        RVK_TRACE("server vkCreatePipelineCache -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineCache: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        RVK_TRACE("server vkDestroyPipelineCache(device={:#x}, pipelineCache={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache));
        s.on_vkDestroyPipelineCache(device, pipelineCache, nullptr);
        RVK_TRACE("server vkDestroyPipelineCache -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateRenderPass(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateRenderPass(device, &pCreateInfo_local, nullptr, &pRenderPass_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkRenderPass(reply, pRenderPass_out);
        RVK_TRACE("server vkCreateRenderPass -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyRenderPass: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPass(r, renderPass)) return false;
        RVK_TRACE("server vkDestroyRenderPass(device={:#x}, renderPass={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(renderPass));
        s.on_vkDestroyRenderPass(device, renderPass, nullptr);
        RVK_TRACE("server vkDestroyRenderPass -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateFramebuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFramebufferCreateInfo pCreateInfo_local{};
        VkFramebuffer pFramebuffer_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFramebufferCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateFramebuffer(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pFramebuffer=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateFramebuffer(device, &pCreateInfo_local, nullptr, &pFramebuffer_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkFramebuffer(reply, pFramebuffer_out);
        RVK_TRACE("server vkCreateFramebuffer -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyFramebuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFramebuffer(r, framebuffer)) return false;
        RVK_TRACE("server vkDestroyFramebuffer(device={:#x}, framebuffer={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(framebuffer));
        s.on_vkDestroyFramebuffer(device, framebuffer, nullptr);
        RVK_TRACE("server vkDestroyFramebuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorSetLayout: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayout pSetLayout_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateDescriptorSetLayout(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pSetLayout=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateDescriptorSetLayout(device, &pCreateInfo_local, nullptr, &pSetLayout_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDescriptorSetLayout(reply, pSetLayout_out);
        RVK_TRACE("server vkCreateDescriptorSetLayout -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorSetLayout: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayout(r, descriptorSetLayout)) return false;
        RVK_TRACE("server vkDestroyDescriptorSetLayout(device={:#x}, descriptorSetLayout={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorSetLayout));
        s.on_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        RVK_TRACE("server vkDestroyDescriptorSetLayout -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPoolCreateInfo pCreateInfo_local{};
        VkDescriptorPool pDescriptorPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateDescriptorPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorPool=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateDescriptorPool(device, &pCreateInfo_local, nullptr, &pDescriptorPool_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDescriptorPool(reply, pDescriptorPool_out);
        RVK_TRACE("server vkCreateDescriptorPool -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        RVK_TRACE("server vkDestroyDescriptorPool(device={:#x}, descriptorPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool));
        s.on_vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        RVK_TRACE("server vkDestroyDescriptorPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeCommandBuffers: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        uint32_t commandBufferCount{};
        std::vector<VkCommandBuffer> pCommandBuffers_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (!r.read_pod(commandBufferCount)) return false;
        pCommandBuffers_storage.resize(static_cast<std::size_t>(commandBufferCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(commandBufferCount); ++__i) {
            if (!deserialize_VkCommandBuffer(r, pCommandBuffers_storage[__i])) return false;
        }
        RVK_TRACE("server vkFreeCommandBuffers(device={:#x}, commandPool={:#x}, commandBufferCount={}, pCommandBuffers=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(commandBufferCount));
        s.on_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers_storage.data());
        RVK_TRACE("server vkFreeCommandBuffers -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit: {
        ::av::WireArena arena;
        (void)arena;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        std::vector<VkSubmitInfo> pSubmits_storage;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage.resize(static_cast<std::size_t>(submitCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
            if (!deserialize_VkSubmitInfo(r, &pSubmits_storage[__i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkQueueSubmit(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
        VkResult result = s.on_vkQueueSubmit(queue, submitCount, pSubmits_storage.data(), fence);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkQueueSubmit -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindPipeline: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        RVK_TRACE("server vkCmdBindPipeline(commandBuffer={:#x}, pipelineBindPoint={}, pipeline={:#x})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(pipeline));
        s.on_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
        RVK_TRACE("server vkCmdBindPipeline -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewport: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstViewport{};
        uint32_t viewportCount{};
        std::vector<VkViewport> pViewports_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstViewport)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage.resize(static_cast<std::size_t>(viewportCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetViewport(commandBuffer={:#x}, firstViewport={}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstViewport), static_cast<long long>(viewportCount));
        s.on_vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports_storage.data());
        RVK_TRACE("server vkCmdSetViewport -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissor: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstScissor{};
        uint32_t scissorCount{};
        std::vector<VkRect2D> pScissors_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstScissor)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage.resize(static_cast<std::size_t>(scissorCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetScissor(commandBuffer={:#x}, firstScissor={}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstScissor), static_cast<long long>(scissorCount));
        s.on_vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors_storage.data());
        RVK_TRACE("server vkCmdSetScissor -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        std::vector<VkBuffer> pBuffers_storage;
        std::vector<VkDeviceSize> pOffsets_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[__i])) return false;
        }
        pOffsets_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!r.read_pod(pOffsets_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdBindVertexBuffers(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
        s.on_vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers_storage.data(), pOffsets_storage.data());
        RVK_TRACE("server vkCmdBindVertexBuffers -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindIndexBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkIndexType indexType{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(indexType)) return false;
        RVK_TRACE("server vkCmdBindIndexBuffer(commandBuffer={:#x}, buffer={:#x}, offset={}, indexType={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(indexType));
        s.on_vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
        RVK_TRACE("server vkCmdBindIndexBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindDescriptorSets: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipelineLayout layout = VK_NULL_HANDLE;
        uint32_t firstSet{};
        uint32_t descriptorSetCount{};
        std::vector<VkDescriptorSet> pDescriptorSets_storage;
        uint32_t dynamicOffsetCount{};
        std::vector<uint32_t> pDynamicOffsets_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (!r.read_pod(firstSet)) return false;
        if (!r.read_pod(descriptorSetCount)) return false;
        pDescriptorSets_storage.resize(static_cast<std::size_t>(descriptorSetCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorSetCount); ++__i) {
            if (!deserialize_VkDescriptorSet(r, pDescriptorSets_storage[__i])) return false;
        }
        if (!r.read_pod(dynamicOffsetCount)) return false;
        pDynamicOffsets_storage.resize(static_cast<std::size_t>(dynamicOffsetCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(dynamicOffsetCount); ++__i) {
            if (!r.read_pod(pDynamicOffsets_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdBindDescriptorSets(commandBuffer={:#x}, pipelineBindPoint={}, layout={:#x}, firstSet={}, descriptorSetCount={}, pDescriptorSets=ptr, dynamicOffsetCount={}, pDynamicOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(layout), static_cast<long long>(firstSet), static_cast<long long>(descriptorSetCount), static_cast<long long>(dynamicOffsetCount));
        s.on_vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets_storage.data(), dynamicOffsetCount, pDynamicOffsets_storage.data());
        RVK_TRACE("server vkCmdBindDescriptorSets -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDraw: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdDraw(commandBuffer={:#x}, vertexCount={}, instanceCount={}, firstVertex={}, firstInstance={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(vertexCount), static_cast<long long>(instanceCount), static_cast<long long>(firstVertex), static_cast<long long>(firstInstance));
        s.on_vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        RVK_TRACE("server vkCmdDraw -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexed: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdDrawIndexed(commandBuffer={:#x}, indexCount={}, instanceCount={}, firstIndex={}, vertexOffset={}, firstInstance={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(indexCount), static_cast<long long>(instanceCount), static_cast<long long>(firstIndex), static_cast<long long>(vertexOffset), static_cast<long long>(firstInstance));
        s.on_vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        RVK_TRACE("server vkCmdDrawIndexed -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags srcStageMask{};
        VkPipelineStageFlags dstStageMask{};
        VkDependencyFlags dependencyFlags{};
        uint32_t memoryBarrierCount{};
        std::vector<VkMemoryBarrier> pMemoryBarriers_storage;
        uint32_t bufferMemoryBarrierCount{};
        std::vector<VkBufferMemoryBarrier> pBufferMemoryBarriers_storage;
        uint32_t imageMemoryBarrierCount{};
        std::vector<VkImageMemoryBarrier> pImageMemoryBarriers_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(srcStageMask)) return false;
        if (!r.read_pod(dstStageMask)) return false;
        if (!r.read_pod(dependencyFlags)) return false;
        if (!r.read_pod(memoryBarrierCount)) return false;
        pMemoryBarriers_storage.resize(static_cast<std::size_t>(memoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryBarrierCount); ++__i) {
            if (!deserialize_VkMemoryBarrier(r, &pMemoryBarriers_storage[__i], arena)) return false;
        }
        if (!r.read_pod(bufferMemoryBarrierCount)) return false;
        pBufferMemoryBarriers_storage.resize(static_cast<std::size_t>(bufferMemoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++__i) {
            if (!deserialize_VkBufferMemoryBarrier(r, &pBufferMemoryBarriers_storage[__i], arena)) return false;
        }
        if (!r.read_pod(imageMemoryBarrierCount)) return false;
        pImageMemoryBarriers_storage.resize(static_cast<std::size_t>(imageMemoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++__i) {
            if (!deserialize_VkImageMemoryBarrier(r, &pImageMemoryBarriers_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdPipelineBarrier(commandBuffer={:#x}, srcStageMask={}, dstStageMask={}, dependencyFlags={}, memoryBarrierCount={}, pMemoryBarriers=ptr, bufferMemoryBarrierCount={}, pBufferMemoryBarriers=ptr, imageMemoryBarrierCount={}, pImageMemoryBarriers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(srcStageMask), static_cast<long long>(dstStageMask), static_cast<long long>(dependencyFlags), static_cast<long long>(memoryBarrierCount), static_cast<long long>(bufferMemoryBarrierCount), static_cast<long long>(imageMemoryBarrierCount));
        s.on_vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers_storage.data(), bufferMemoryBarrierCount, pBufferMemoryBarriers_storage.data(), imageMemoryBarrierCount, pImageMemoryBarriers_storage.data());
        RVK_TRACE("server vkCmdPipelineBarrier -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer srcBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        uint32_t regionCount{};
        std::vector<VkBufferCopy> pRegions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, srcBuffer)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkBufferCopy(r, &pRegions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdCopyBuffer(commandBuffer={:#x}, srcBuffer={:#x}, dstBuffer={:#x}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(regionCount));
        s.on_vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions_storage.data());
        RVK_TRACE("server vkCmdCopyBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassContents contents{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!r.read_pod(contents)) return false;
        RVK_TRACE("server vkCmdBeginRenderPass(commandBuffer={:#x}, pRenderPassBegin=ptr, contents={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(contents));
        s.on_vkCmdBeginRenderPass(commandBuffer, &pRenderPassBegin_local, contents);
        RVK_TRACE("server vkCmdBeginRenderPass -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        RVK_TRACE("server vkCmdEndRenderPass(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndRenderPass(commandBuffer);
        RVK_TRACE("server vkCmdEndRenderPass -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassContents contents{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(contents)) return false;
        RVK_TRACE("server vkCmdNextSubpass(commandBuffer={:#x}, contents={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(contents));
        s.on_vkCmdNextSubpass(commandBuffer, contents);
        RVK_TRACE("server vkCmdNextSubpass -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushConstants: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkShaderStageFlags stageFlags{};
        uint32_t offset{};
        uint32_t size{};
        std::vector<std::uint8_t> pValues_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (!r.read_pod(stageFlags)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(size)) return false;
        if (size > 0) {
            pValues_storage.resize(static_cast<std::size_t>(size));
            if (!r.read(pValues_storage.data(), static_cast<std::size_t>(size))) return false;
        }
        RVK_TRACE("server vkCmdPushConstants(commandBuffer={:#x}, layout={:#x}, stageFlags={}, offset={}, size={}, pValues=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(layout), static_cast<long long>(stageFlags), static_cast<long long>(offset), static_cast<long long>(size));
        s.on_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues_storage.data());
        RVK_TRACE("server vkCmdPushConstants -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateGraphicsPipelines: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        std::vector<VkGraphicsPipelineCreateInfo> pCreateInfos_storage;
        std::vector<VkPipeline> pPipelines_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage.resize(static_cast<std::size_t>(createInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
            if (!deserialize_VkGraphicsPipelineCreateInfo(r, &pCreateInfos_storage[__i], arena)) return false;
        }
        pPipelines_storage.resize(static_cast<std::size_t>(createInfoCount));
        RVK_TRACE("server vkCreateGraphicsPipelines(device={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
        VkResult result = s.on_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos_storage.data(), nullptr, pPipelines_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pPipelines_storage.size(); ++__k) {
            serialize_VkPipeline(reply, pPipelines_storage[__k]);
        }
        RVK_TRACE("server vkCreateGraphicsPipelines -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkUpdateDescriptorSets: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t descriptorWriteCount{};
        std::vector<VkWriteDescriptorSet> pDescriptorWrites_storage;
        uint32_t descriptorCopyCount{};
        std::vector<VkCopyDescriptorSet> pDescriptorCopies_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(descriptorWriteCount)) return false;
        pDescriptorWrites_storage.resize(static_cast<std::size_t>(descriptorWriteCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorWriteCount); ++__i) {
            if (!deserialize_VkWriteDescriptorSet(r, &pDescriptorWrites_storage[__i], arena)) return false;
        }
        if (!r.read_pod(descriptorCopyCount)) return false;
        pDescriptorCopies_storage.resize(static_cast<std::size_t>(descriptorCopyCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorCopyCount); ++__i) {
            if (!deserialize_VkCopyDescriptorSet(r, &pDescriptorCopies_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkUpdateDescriptorSets(device={:#x}, descriptorWriteCount={}, pDescriptorWrites=ptr, descriptorCopyCount={}, pDescriptorCopies=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(descriptorWriteCount), static_cast<long long>(descriptorCopyCount));
        s.on_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites_storage.data(), descriptorCopyCount, pDescriptorCopies_storage.data());
        RVK_TRACE("server vkUpdateDescriptorSets -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateCommandBuffers: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo pAllocateInfo_local{};
        std::vector<VkCommandBuffer> pCommandBuffers_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandBufferAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        pCommandBuffers_storage.resize(static_cast<std::size_t>(pAllocateInfo_local.commandBufferCount));
        RVK_TRACE("server vkAllocateCommandBuffers(device={:#x}, pAllocateInfo=ptr, pCommandBuffers=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkAllocateCommandBuffers(device, &pAllocateInfo_local, pCommandBuffers_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pCommandBuffers_storage.size(); ++__k) {
            serialize_VkCommandBuffer(reply, pCommandBuffers_storage[__k]);
        }
        RVK_TRACE("server vkAllocateCommandBuffers -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkAllocateDescriptorSets: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo pAllocateInfo_local{};
        std::vector<VkDescriptorSet> pDescriptorSets_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetAllocateInfo(r, &pAllocateInfo_local, arena)) return false;
        pDescriptorSets_storage.resize(static_cast<std::size_t>(pAllocateInfo_local.descriptorSetCount));
        RVK_TRACE("server vkAllocateDescriptorSets(device={:#x}, pAllocateInfo=ptr, pDescriptorSets=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkAllocateDescriptorSets(device, &pAllocateInfo_local, pDescriptorSets_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pDescriptorSets_storage.size(); ++__k) {
            serialize_VkDescriptorSet(reply, pDescriptorSets_storage[__k]);
        }
        RVK_TRACE("server vkAllocateDescriptorSets -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkFreeDescriptorSets: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        uint32_t descriptorSetCount{};
        std::vector<VkDescriptorSet> pDescriptorSets_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        if (!r.read_pod(descriptorSetCount)) return false;
        pDescriptorSets_storage.resize(static_cast<std::size_t>(descriptorSetCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorSetCount); ++__i) {
            if (!deserialize_VkDescriptorSet(r, pDescriptorSets_storage[__i])) return false;
        }
        RVK_TRACE("server vkFreeDescriptorSets(device={:#x}, descriptorPool={:#x}, descriptorSetCount={}, pDescriptorSets=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool), static_cast<long long>(descriptorSetCount));
        VkResult result = s.on_vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkFreeDescriptorSets -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateComputePipelines: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        std::vector<VkComputePipelineCreateInfo> pCreateInfos_storage;
        std::vector<VkPipeline> pPipelines_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage.resize(static_cast<std::size_t>(createInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
            if (!deserialize_VkComputePipelineCreateInfo(r, &pCreateInfos_storage[__i], arena)) return false;
        }
        pPipelines_storage.resize(static_cast<std::size_t>(createInfoCount));
        RVK_TRACE("server vkCreateComputePipelines(device={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
        VkResult result = s.on_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos_storage.data(), nullptr, pPipelines_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pPipelines_storage.size(); ++__k) {
            serialize_VkPipeline(reply, pPipelines_storage[__k]);
        }
        RVK_TRACE("server vkCreateComputePipelines -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetDescriptorPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorPoolResetFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorPool(r, descriptorPool)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkResetDescriptorPool(device={:#x}, descriptorPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorPool), static_cast<long long>(flags));
        VkResult result = s.on_vkResetDescriptorPool(device, descriptorPool, flags);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkResetDescriptorPool -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkEventCreateInfo pCreateInfo_local{};
        VkEvent pEvent_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEventCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateEvent(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pEvent=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateEvent(device, &pCreateInfo_local, nullptr, &pEvent_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkEvent(reply, pEvent_out);
        RVK_TRACE("server vkCreateEvent -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        RVK_TRACE("server vkDestroyEvent(device={:#x}, event={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
        s.on_vkDestroyEvent(device, event, nullptr);
        RVK_TRACE("server vkDestroyEvent -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetEventStatus: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        RVK_TRACE("server vkGetEventStatus(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
        VkResult result = s.on_vkGetEventStatus(device, event);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkGetEventStatus -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSetEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        RVK_TRACE("server vkSetEvent(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
        VkResult result = s.on_vkSetEvent(device, event);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkSetEvent -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        RVK_TRACE("server vkResetEvent(device={:#x}, event={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(event));
        VkResult result = s.on_vkResetEvent(device, event);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkResetEvent -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!r.read_pod(stageMask)) return false;
        RVK_TRACE("server vkCmdSetEvent(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
        s.on_vkCmdSetEvent(commandBuffer, event, stageMask);
        RVK_TRACE("server vkCmdSetEvent -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!r.read_pod(stageMask)) return false;
        RVK_TRACE("server vkCmdResetEvent(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
        s.on_vkCmdResetEvent(commandBuffer, event, stageMask);
        RVK_TRACE("server vkCmdResetEvent -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        std::vector<VkEvent> pEvents_storage;
        VkPipelineStageFlags srcStageMask{};
        VkPipelineStageFlags dstStageMask{};
        uint32_t memoryBarrierCount{};
        std::vector<VkMemoryBarrier> pMemoryBarriers_storage;
        uint32_t bufferMemoryBarrierCount{};
        std::vector<VkBufferMemoryBarrier> pBufferMemoryBarriers_storage;
        uint32_t imageMemoryBarrierCount{};
        std::vector<VkImageMemoryBarrier> pImageMemoryBarriers_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage.resize(static_cast<std::size_t>(eventCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
            if (!deserialize_VkEvent(r, pEvents_storage[__i])) return false;
        }
        if (!r.read_pod(srcStageMask)) return false;
        if (!r.read_pod(dstStageMask)) return false;
        if (!r.read_pod(memoryBarrierCount)) return false;
        pMemoryBarriers_storage.resize(static_cast<std::size_t>(memoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryBarrierCount); ++__i) {
            if (!deserialize_VkMemoryBarrier(r, &pMemoryBarriers_storage[__i], arena)) return false;
        }
        if (!r.read_pod(bufferMemoryBarrierCount)) return false;
        pBufferMemoryBarriers_storage.resize(static_cast<std::size_t>(bufferMemoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bufferMemoryBarrierCount); ++__i) {
            if (!deserialize_VkBufferMemoryBarrier(r, &pBufferMemoryBarriers_storage[__i], arena)) return false;
        }
        if (!r.read_pod(imageMemoryBarrierCount)) return false;
        pImageMemoryBarriers_storage.resize(static_cast<std::size_t>(imageMemoryBarrierCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(imageMemoryBarrierCount); ++__i) {
            if (!deserialize_VkImageMemoryBarrier(r, &pImageMemoryBarriers_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdWaitEvents(commandBuffer={:#x}, eventCount={}, pEvents=ptr, srcStageMask={}, dstStageMask={}, memoryBarrierCount={}, pMemoryBarriers=ptr, bufferMemoryBarrierCount={}, pBufferMemoryBarriers=ptr, imageMemoryBarrierCount={}, pImageMemoryBarriers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount), static_cast<long long>(srcStageMask), static_cast<long long>(dstStageMask), static_cast<long long>(memoryBarrierCount), static_cast<long long>(bufferMemoryBarrierCount), static_cast<long long>(imageMemoryBarrierCount));
        s.on_vkCmdWaitEvents(commandBuffer, eventCount, pEvents_storage.data(), srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers_storage.data(), bufferMemoryBarrierCount, pBufferMemoryBarriers_storage.data(), imageMemoryBarrierCount, pImageMemoryBarriers_storage.data());
        RVK_TRACE("server vkCmdWaitEvents -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        std::vector<VkImageCopy> pRegions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkImageCopy(r, &pRegions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdCopyImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
        s.on_vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage.data());
        RVK_TRACE("server vkCmdCopyImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer srcBuffer = VK_NULL_HANDLE;
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        std::vector<VkBufferImageCopy> pRegions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, srcBuffer)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkBufferImageCopy(r, &pRegions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdCopyBufferToImage(commandBuffer={:#x}, srcBuffer={:#x}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcBuffer), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
        s.on_vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions_storage.data());
        RVK_TRACE("server vkCmdCopyBufferToImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        uint32_t regionCount{};
        std::vector<VkBufferImageCopy> pRegions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkBufferImageCopy(r, &pRegions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdCopyImageToBuffer(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstBuffer={:#x}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(regionCount));
        s.on_vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions_storage.data());
        RVK_TRACE("server vkCmdCopyImageToBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        std::vector<VkImageBlit> pRegions_storage;
        VkFilter filter{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkImageBlit(r, &pRegions_storage[__i], arena)) return false;
        }
        if (!r.read_pod(filter)) return false;
        RVK_TRACE("server vkCmdBlitImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr, filter={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount), static_cast<long long>(filter));
        s.on_vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage.data(), filter);
        RVK_TRACE("server vkCmdBlitImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearColorImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout imageLayout{};
        VkClearColorValue pColor_local{};
        uint32_t rangeCount{};
        std::vector<VkImageSubresourceRange> pRanges_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!r.read_pod(imageLayout)) return false;
        if (!deserialize_VkClearColorValue(r, &pColor_local, arena)) return false;
        if (!r.read_pod(rangeCount)) return false;
        pRanges_storage.resize(static_cast<std::size_t>(rangeCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rangeCount); ++__i) {
            if (!deserialize_VkImageSubresourceRange(r, &pRanges_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdClearColorImage(commandBuffer={:#x}, image={:#x}, imageLayout={}, pColor=ptr, rangeCount={}, pRanges=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(image), static_cast<long long>(imageLayout), static_cast<long long>(rangeCount));
        s.on_vkCmdClearColorImage(commandBuffer, image, imageLayout, &pColor_local, rangeCount, pRanges_storage.data());
        RVK_TRACE("server vkCmdClearColorImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearDepthStencilImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout imageLayout{};
        VkClearDepthStencilValue pDepthStencil_local{};
        uint32_t rangeCount{};
        std::vector<VkImageSubresourceRange> pRanges_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!r.read_pod(imageLayout)) return false;
        if (!deserialize_VkClearDepthStencilValue(r, &pDepthStencil_local, arena)) return false;
        if (!r.read_pod(rangeCount)) return false;
        pRanges_storage.resize(static_cast<std::size_t>(rangeCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rangeCount); ++__i) {
            if (!deserialize_VkImageSubresourceRange(r, &pRanges_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdClearDepthStencilImage(commandBuffer={:#x}, image={:#x}, imageLayout={}, pDepthStencil=ptr, rangeCount={}, pRanges=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(image), static_cast<long long>(imageLayout), static_cast<long long>(rangeCount));
        s.on_vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, &pDepthStencil_local, rangeCount, pRanges_storage.data());
        RVK_TRACE("server vkCmdClearDepthStencilImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdClearAttachments: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t attachmentCount{};
        std::vector<VkClearAttachment> pAttachments_storage;
        uint32_t rectCount{};
        std::vector<VkClearRect> pRects_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pAttachments_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!deserialize_VkClearAttachment(r, &pAttachments_storage[__i], arena)) return false;
        }
        if (!r.read_pod(rectCount)) return false;
        pRects_storage.resize(static_cast<std::size_t>(rectCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(rectCount); ++__i) {
            if (!deserialize_VkClearRect(r, &pRects_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdClearAttachments(commandBuffer={:#x}, attachmentCount={}, pAttachments=ptr, rectCount={}, pRects=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(attachmentCount), static_cast<long long>(rectCount));
        s.on_vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments_storage.data(), rectCount, pRects_storage.data());
        RVK_TRACE("server vkCmdClearAttachments -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdFillBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        VkDeviceSize size{};
        uint32_t data{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(size)) return false;
        if (!r.read_pod(data)) return false;
        RVK_TRACE("server vkCmdFillBuffer(commandBuffer={:#x}, dstBuffer={:#x}, dstOffset={}, size={}, data={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(size), static_cast<long long>(data));
        s.on_vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
        RVK_TRACE("server vkCmdFillBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdUpdateBuffer: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer dstBuffer = VK_NULL_HANDLE;
        VkDeviceSize dstOffset{};
        VkDeviceSize dataSize{};
        std::vector<std::uint8_t> pData_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(dataSize)) return false;
        if (dataSize > 0) {
            pData_storage.resize(static_cast<std::size_t>(dataSize));
            if (!r.read(pData_storage.data(), static_cast<std::size_t>(dataSize))) return false;
        }
        RVK_TRACE("server vkCmdUpdateBuffer(commandBuffer={:#x}, dstBuffer={:#x}, dstOffset={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(dataSize));
        s.on_vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData_storage.data());
        RVK_TRACE("server vkCmdUpdateBuffer -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineWidth: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float lineWidth{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineWidth)) return false;
        RVK_TRACE("server vkCmdSetLineWidth(commandBuffer={:#x}, lineWidth={})",::rvk::instr::trace_handle(commandBuffer), lineWidth);
        s.on_vkCmdSetLineWidth(commandBuffer, lineWidth);
        RVK_TRACE("server vkCmdSetLineWidth -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBias: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float depthBiasConstantFactor{};
        float depthBiasClamp{};
        float depthBiasSlopeFactor{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasConstantFactor)) return false;
        if (!r.read_pod(depthBiasClamp)) return false;
        if (!r.read_pod(depthBiasSlopeFactor)) return false;
        RVK_TRACE("server vkCmdSetDepthBias(commandBuffer={:#x}, depthBiasConstantFactor={}, depthBiasClamp={}, depthBiasSlopeFactor={})",::rvk::instr::trace_handle(commandBuffer), depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        s.on_vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        RVK_TRACE("server vkCmdSetDepthBias -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetBlendConstants: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float blendConstants_local[4]{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        for (std::uint32_t __i = 0; __i < 4u; ++__i) {
            if (!r.read_pod(blendConstants_local[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetBlendConstants(commandBuffer={:#x}, blendConstants=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdSetBlendConstants(commandBuffer, blendConstants_local);
        RVK_TRACE("server vkCmdSetBlendConstants -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBounds: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float minDepthBounds{};
        float maxDepthBounds{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(minDepthBounds)) return false;
        if (!r.read_pod(maxDepthBounds)) return false;
        RVK_TRACE("server vkCmdSetDepthBounds(commandBuffer={:#x}, minDepthBounds={}, maxDepthBounds={})",::rvk::instr::trace_handle(commandBuffer), minDepthBounds, maxDepthBounds);
        s.on_vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
        RVK_TRACE("server vkCmdSetDepthBounds -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilCompareMask: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t compareMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(compareMask)) return false;
        RVK_TRACE("server vkCmdSetStencilCompareMask(commandBuffer={:#x}, faceMask={}, compareMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(compareMask));
        s.on_vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
        RVK_TRACE("server vkCmdSetStencilCompareMask -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilWriteMask: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t writeMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(writeMask)) return false;
        RVK_TRACE("server vkCmdSetStencilWriteMask(commandBuffer={:#x}, faceMask={}, writeMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(writeMask));
        s.on_vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
        RVK_TRACE("server vkCmdSetStencilWriteMask -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilReference: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkStencilFaceFlags faceMask{};
        uint32_t reference{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(faceMask)) return false;
        if (!r.read_pod(reference)) return false;
        RVK_TRACE("server vkCmdSetStencilReference(commandBuffer={:#x}, faceMask={}, reference={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(reference));
        s.on_vkCmdSetStencilReference(commandBuffer, faceMask, reference);
        RVK_TRACE("server vkCmdSetStencilReference -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatch: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        RVK_TRACE("server vkCmdDispatch(commandBuffer={:#x}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
        s.on_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
        RVK_TRACE("server vkCmdDispatch -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchIndirect: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        RVK_TRACE("server vkCmdDispatchIndirect(commandBuffer={:#x}, buffer={:#x}, offset={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset));
        s.on_vkCmdDispatchIndirect(commandBuffer, buffer, offset);
        RVK_TRACE("server vkCmdDispatchIndirect -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirect: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndirect(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
        RVK_TRACE("server vkCmdDrawIndirect -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirect: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndexedIndirect(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
        RVK_TRACE("server vkCmdDrawIndexedIndirect -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdExecuteCommands: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t commandBufferCount{};
        std::vector<VkCommandBuffer> pCommandBuffers_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(commandBufferCount)) return false;
        pCommandBuffers_storage.resize(static_cast<std::size_t>(commandBufferCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(commandBufferCount); ++__i) {
            if (!deserialize_VkCommandBuffer(r, pCommandBuffers_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdExecuteCommands(commandBuffer={:#x}, commandBufferCount={}, pCommandBuffers=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(commandBufferCount));
        s.on_vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers_storage.data());
        RVK_TRACE("server vkCmdExecuteCommands -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkAcquireNextImage2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAcquireNextImageInfoKHR pAcquireInfo_local{};
        uint32_t pImageIndex_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAcquireNextImageInfoKHR(r, &pAcquireInfo_local, arena)) return false;
        RVK_TRACE("server vkAcquireNextImage2KHR(device={:#x}, pAcquireInfo=ptr, pImageIndex=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkAcquireNextImage2KHR(device, &pAcquireInfo_local, &pImageIndex_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pImageIndex_out);
        RVK_TRACE("server vkAcquireNextImage2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        std::vector<VkBindBufferMemoryInfo> pBindInfos_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage.resize(static_cast<std::size_t>(bindInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
            if (!deserialize_VkBindBufferMemoryInfo(r, &pBindInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkBindBufferMemory2(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
        VkResult result = s.on_vkBindBufferMemory2(device, bindInfoCount, pBindInfos_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindBufferMemory2 -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        std::vector<VkBindImageMemoryInfo> pBindInfos_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage.resize(static_cast<std::size_t>(bindInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
            if (!deserialize_VkBindImageMemoryInfo(r, &pBindInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkBindImageMemory2(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
        VkResult result = s.on_vkBindImageMemory2(device, bindInfoCount, pBindInfos_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindImageMemory2 -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginQuery: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        VkQueryControlFlags flags{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkCmdBeginQuery(commandBuffer={:#x}, queryPool={:#x}, query={}, flags={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(flags));
        s.on_vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
        RVK_TRACE("server vkCmdBeginQuery -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBeginRenderPass2(commandBuffer={:#x}, pRenderPassBegin=ptr, pSubpassBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBeginRenderPass2(commandBuffer, &pRenderPassBegin_local, &pSubpassBeginInfo_local);
        RVK_TRACE("server vkCmdBeginRenderPass2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRendering: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInfo pRenderingInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInfo(r, &pRenderingInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBeginRendering(commandBuffer={:#x}, pRenderingInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBeginRendering(commandBuffer, &pRenderingInfo_local);
        RVK_TRACE("server vkCmdBeginRendering -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        std::vector<VkBuffer> pBuffers_storage;
        std::vector<VkDeviceSize> pOffsets_storage;
        std::vector<VkDeviceSize> pSizes_storage;
        std::uint8_t pSizes_present = 0;
        std::vector<VkDeviceSize> pStrides_storage;
        std::uint8_t pStrides_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[__i])) return false;
        }
        pOffsets_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!r.read_pod(pOffsets_storage[__i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage.resize(static_cast<std::size_t>(bindingCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
                if (!r.read_pod(pSizes_storage[__i])) return false;
            }
        }
        if (!r.read_pod(pStrides_present)) return false;
        if (pStrides_present) {
            pStrides_storage.resize(static_cast<std::size_t>(bindingCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
                if (!r.read_pod(pStrides_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdBindVertexBuffers2(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr, pStrides=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
        s.on_vkCmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers_storage.data(), pOffsets_storage.data(), pSizes_present ? pSizes_storage.data() : nullptr, pStrides_present ? pStrides_storage.data() : nullptr);
        RVK_TRACE("server vkCmdBindVertexBuffers2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBlitImageInfo2 pBlitImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBlitImageInfo2(r, &pBlitImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBlitImage2(commandBuffer={:#x}, pBlitImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBlitImage2(commandBuffer, &pBlitImageInfo_local);
        RVK_TRACE("server vkCmdBlitImage2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferInfo2 pCopyBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferInfo2(r, &pCopyBufferInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyBuffer2(commandBuffer={:#x}, pCopyBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyBuffer2(commandBuffer, &pCopyBufferInfo_local);
        RVK_TRACE("server vkCmdCopyBuffer2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferToImageInfo2 pCopyBufferToImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferToImageInfo2(r, &pCopyBufferToImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyBufferToImage2(commandBuffer={:#x}, pCopyBufferToImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyBufferToImage2(commandBuffer, &pCopyBufferToImageInfo_local);
        RVK_TRACE("server vkCmdCopyBufferToImage2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageInfo2 pCopyImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageInfo2(r, &pCopyImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyImage2(commandBuffer={:#x}, pCopyImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyImage2(commandBuffer, &pCopyImageInfo_local);
        RVK_TRACE("server vkCmdCopyImage2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageToBufferInfo2 pCopyImageToBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageToBufferInfo2(r, &pCopyImageToBufferInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyImageToBuffer2(commandBuffer={:#x}, pCopyImageToBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyImageToBuffer2(commandBuffer, &pCopyImageToBufferInfo_local);
        RVK_TRACE("server vkCmdCopyImageToBuffer2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyQueryPoolResults: {
        ::av::WireArena arena;
        (void)arena;
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
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        if (!deserialize_VkBuffer(r, dstBuffer)) return false;
        if (!r.read_pod(dstOffset)) return false;
        if (!r.read_pod(stride)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkCmdCopyQueryPoolResults(commandBuffer={:#x}, queryPool={:#x}, firstQuery={}, queryCount={}, dstBuffer={:#x}, dstOffset={}, stride={}, flags={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount), ::rvk::instr::trace_handle(dstBuffer), static_cast<long long>(dstOffset), static_cast<long long>(stride), static_cast<long long>(flags));
        s.on_vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
        RVK_TRACE("server vkCmdCopyQueryPoolResults -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchBase: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdDispatchBase(commandBuffer={:#x}, baseGroupX={}, baseGroupY={}, baseGroupZ={}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(baseGroupX), static_cast<long long>(baseGroupY), static_cast<long long>(baseGroupZ), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
        s.on_vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        RVK_TRACE("server vkCmdDispatchBase -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCount: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndexedIndirectCount(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        RVK_TRACE("server vkCmdDrawIndexedIndirectCount -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectCount: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndirectCount(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        RVK_TRACE("server vkCmdDrawIndirectCount -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndQuery: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        RVK_TRACE("server vkCmdEndQuery(commandBuffer={:#x}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
        s.on_vkCmdEndQuery(commandBuffer, queryPool, query);
        RVK_TRACE("server vkCmdEndQuery -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdEndRenderPass2(commandBuffer={:#x}, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndRenderPass2(commandBuffer, &pSubpassEndInfo_local);
        RVK_TRACE("server vkCmdEndRenderPass2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRendering: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        RVK_TRACE("server vkCmdEndRendering(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndRendering(commandBuffer);
        RVK_TRACE("server vkCmdEndRendering -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdNextSubpass2(commandBuffer={:#x}, pSubpassBeginInfo=ptr, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdNextSubpass2(commandBuffer, &pSubpassBeginInfo_local, &pSubpassEndInfo_local);
        RVK_TRACE("server vkCmdNextSubpass2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdPipelineBarrier2(commandBuffer={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdPipelineBarrier2(commandBuffer, &pDependencyInfo_local);
        RVK_TRACE("server vkCmdPipelineBarrier2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!r.read_pod(stageMask)) return false;
        RVK_TRACE("server vkCmdResetEvent2(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
        s.on_vkCmdResetEvent2(commandBuffer, event, stageMask);
        RVK_TRACE("server vkCmdResetEvent2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetQueryPool: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        RVK_TRACE("server vkCmdResetQueryPool(commandBuffer={:#x}, queryPool={:#x}, firstQuery={}, queryCount={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount));
        s.on_vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
        RVK_TRACE("server vkCmdResetQueryPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkImage srcImage = VK_NULL_HANDLE;
        VkImageLayout srcImageLayout{};
        VkImage dstImage = VK_NULL_HANDLE;
        VkImageLayout dstImageLayout{};
        uint32_t regionCount{};
        std::vector<VkImageResolve> pRegions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkImage(r, srcImage)) return false;
        if (!r.read_pod(srcImageLayout)) return false;
        if (!deserialize_VkImage(r, dstImage)) return false;
        if (!r.read_pod(dstImageLayout)) return false;
        if (!r.read_pod(regionCount)) return false;
        pRegions_storage.resize(static_cast<std::size_t>(regionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(regionCount); ++__i) {
            if (!deserialize_VkImageResolve(r, &pRegions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdResolveImage(commandBuffer={:#x}, srcImage={:#x}, srcImageLayout={}, dstImage={:#x}, dstImageLayout={}, regionCount={}, pRegions=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(srcImage), static_cast<long long>(srcImageLayout), ::rvk::instr::trace_handle(dstImage), static_cast<long long>(dstImageLayout), static_cast<long long>(regionCount));
        s.on_vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions_storage.data());
        RVK_TRACE("server vkCmdResolveImage -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResolveImageInfo2 pResolveImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkResolveImageInfo2(r, &pResolveImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdResolveImage2(commandBuffer={:#x}, pResolveImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdResolveImage2(commandBuffer, &pResolveImageInfo_local);
        RVK_TRACE("server vkCmdResolveImage2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCullMode: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCullModeFlags cullMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(cullMode)) return false;
        RVK_TRACE("server vkCmdSetCullMode(commandBuffer={:#x}, cullMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(cullMode));
        s.on_vkCmdSetCullMode(commandBuffer, cullMode);
        RVK_TRACE("server vkCmdSetCullMode -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBiasEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBiasEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthBiasEnable(commandBuffer={:#x}, depthBiasEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBiasEnable));
        s.on_vkCmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
        RVK_TRACE("server vkCmdSetDepthBiasEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBoundsTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBoundsTestEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthBoundsTestEnable(commandBuffer={:#x}, depthBoundsTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBoundsTestEnable));
        s.on_vkCmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
        RVK_TRACE("server vkCmdSetDepthBoundsTestEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthCompareOp: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCompareOp depthCompareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthCompareOp)) return false;
        RVK_TRACE("server vkCmdSetDepthCompareOp(commandBuffer={:#x}, depthCompareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthCompareOp));
        s.on_vkCmdSetDepthCompareOp(commandBuffer, depthCompareOp);
        RVK_TRACE("server vkCmdSetDepthCompareOp -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthTestEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthTestEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthTestEnable(commandBuffer={:#x}, depthTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthTestEnable));
        s.on_vkCmdSetDepthTestEnable(commandBuffer, depthTestEnable);
        RVK_TRACE("server vkCmdSetDepthTestEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthWriteEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthWriteEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthWriteEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthWriteEnable(commandBuffer={:#x}, depthWriteEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthWriteEnable));
        s.on_vkCmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
        RVK_TRACE("server vkCmdSetDepthWriteEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDeviceMask: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t deviceMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(deviceMask)) return false;
        RVK_TRACE("server vkCmdSetDeviceMask(commandBuffer={:#x}, deviceMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(deviceMask));
        s.on_vkCmdSetDeviceMask(commandBuffer, deviceMask);
        RVK_TRACE("server vkCmdSetDeviceMask -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdSetEvent2(commandBuffer={:#x}, event={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event));
        s.on_vkCmdSetEvent2(commandBuffer, event, &pDependencyInfo_local);
        RVK_TRACE("server vkCmdSetEvent2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFrontFace: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFrontFace frontFace{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(frontFace)) return false;
        RVK_TRACE("server vkCmdSetFrontFace(commandBuffer={:#x}, frontFace={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(frontFace));
        s.on_vkCmdSetFrontFace(commandBuffer, frontFace);
        RVK_TRACE("server vkCmdSetFrontFace -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 primitiveRestartEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveRestartEnable)) return false;
        RVK_TRACE("server vkCmdSetPrimitiveRestartEnable(commandBuffer={:#x}, primitiveRestartEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveRestartEnable));
        s.on_vkCmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
        RVK_TRACE("server vkCmdSetPrimitiveRestartEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveTopology: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPrimitiveTopology primitiveTopology{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveTopology)) return false;
        RVK_TRACE("server vkCmdSetPrimitiveTopology(commandBuffer={:#x}, primitiveTopology={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveTopology));
        s.on_vkCmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
        RVK_TRACE("server vkCmdSetPrimitiveTopology -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 rasterizerDiscardEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizerDiscardEnable)) return false;
        RVK_TRACE("server vkCmdSetRasterizerDiscardEnable(commandBuffer={:#x}, rasterizerDiscardEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizerDiscardEnable));
        s.on_vkCmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
        RVK_TRACE("server vkCmdSetRasterizerDiscardEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissorWithCount: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t scissorCount{};
        std::vector<VkRect2D> pScissors_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage.resize(static_cast<std::size_t>(scissorCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetScissorWithCount(commandBuffer={:#x}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(scissorCount));
        s.on_vkCmdSetScissorWithCount(commandBuffer, scissorCount, pScissors_storage.data());
        RVK_TRACE("server vkCmdSetScissorWithCount -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilOp: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdSetStencilOp(commandBuffer={:#x}, faceMask={}, failOp={}, passOp={}, depthFailOp={}, compareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(failOp), static_cast<long long>(passOp), static_cast<long long>(depthFailOp), static_cast<long long>(compareOp));
        s.on_vkCmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
        RVK_TRACE("server vkCmdSetStencilOp -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilTestEnable: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stencilTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stencilTestEnable)) return false;
        RVK_TRACE("server vkCmdSetStencilTestEnable(commandBuffer={:#x}, stencilTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stencilTestEnable));
        s.on_vkCmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
        RVK_TRACE("server vkCmdSetStencilTestEnable -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWithCount: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t viewportCount{};
        std::vector<VkViewport> pViewports_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage.resize(static_cast<std::size_t>(viewportCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetViewportWithCount(commandBuffer={:#x}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportCount));
        s.on_vkCmdSetViewportWithCount(commandBuffer, viewportCount, pViewports_storage.data());
        RVK_TRACE("server vkCmdSetViewportWithCount -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        std::vector<VkEvent> pEvents_storage;
        std::vector<VkDependencyInfo> pDependencyInfos_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage.resize(static_cast<std::size_t>(eventCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
            if (!deserialize_VkEvent(r, pEvents_storage[__i])) return false;
        }
        pDependencyInfos_storage.resize(static_cast<std::size_t>(eventCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
            if (!deserialize_VkDependencyInfo(r, &pDependencyInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdWaitEvents2(commandBuffer={:#x}, eventCount={}, pEvents=ptr, pDependencyInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount));
        s.on_vkCmdWaitEvents2(commandBuffer, eventCount, pEvents_storage.data(), pDependencyInfos_storage.data());
        RVK_TRACE("server vkCmdWaitEvents2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlagBits pipelineStage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineStage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        RVK_TRACE("server vkCmdWriteTimestamp(commandBuffer={:#x}, pipelineStage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineStage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
        s.on_vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
        RVK_TRACE("server vkCmdWriteTimestamp -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp2: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        RVK_TRACE("server vkCmdWriteTimestamp2(commandBuffer={:#x}, stage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
        s.on_vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
        RVK_TRACE("server vkCmdWriteTimestamp2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateBufferView: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferViewCreateInfo pCreateInfo_local{};
        VkBufferView pView_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferViewCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateBufferView(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pView=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateBufferView(device, &pCreateInfo_local, nullptr, &pView_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkBufferView(reply, pView_out);
        RVK_TRACE("server vkCreateBufferView -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplate: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplateCreateInfo pCreateInfo_local{};
        VkDescriptorUpdateTemplate pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplateCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateDescriptorUpdateTemplate(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorUpdateTemplate=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateDescriptorUpdateTemplate(device, &pCreateInfo_local, nullptr, &pDescriptorUpdateTemplate_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDescriptorUpdateTemplate(reply, pDescriptorUpdateTemplate_out);
        RVK_TRACE("server vkCreateDescriptorUpdateTemplate -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePrivateDataSlot: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPrivateDataSlotCreateInfo pCreateInfo_local{};
        VkPrivateDataSlot pPrivateDataSlot_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPrivateDataSlotCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreatePrivateDataSlot(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pPrivateDataSlot=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreatePrivateDataSlot(device, &pCreateInfo_local, nullptr, &pPrivateDataSlot_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkPrivateDataSlot(reply, pPrivateDataSlot_out);
        RVK_TRACE("server vkCreatePrivateDataSlot -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateQueryPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPoolCreateInfo pCreateInfo_local{};
        VkQueryPool pQueryPool_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPoolCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateQueryPool(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pQueryPool=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateQueryPool(device, &pCreateInfo_local, nullptr, &pQueryPool_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkQueryPool(reply, pQueryPool_out);
        RVK_TRACE("server vkCreateQueryPool -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo2 pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo2(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateRenderPass2(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateRenderPass2(device, &pCreateInfo_local, nullptr, &pRenderPass_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkRenderPass(reply, pRenderPass_out);
        RVK_TRACE("server vkCreateRenderPass2 -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSamplerYcbcrConversion: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversionCreateInfo pCreateInfo_local{};
        VkSamplerYcbcrConversion pYcbcrConversion_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversionCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateSamplerYcbcrConversion(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pYcbcrConversion=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateSamplerYcbcrConversion(device, &pCreateInfo_local, nullptr, &pYcbcrConversion_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkSamplerYcbcrConversion(reply, pYcbcrConversion_out);
        RVK_TRACE("server vkCreateSamplerYcbcrConversion -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyBufferView: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferView bufferView = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferView(r, bufferView)) return false;
        RVK_TRACE("server vkDestroyBufferView(device={:#x}, bufferView={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(bufferView));
        s.on_vkDestroyBufferView(device, bufferView, nullptr);
        RVK_TRACE("server vkDestroyBufferView -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplate: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        RVK_TRACE("server vkDestroyDescriptorUpdateTemplate(device={:#x}, descriptorUpdateTemplate={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorUpdateTemplate));
        s.on_vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
        RVK_TRACE("server vkDestroyDescriptorUpdateTemplate -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPrivateDataSlot: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        RVK_TRACE("server vkDestroyPrivateDataSlot(device={:#x}, privateDataSlot={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(privateDataSlot));
        s.on_vkDestroyPrivateDataSlot(device, privateDataSlot, nullptr);
        RVK_TRACE("server vkDestroyPrivateDataSlot -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyQueryPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        RVK_TRACE("server vkDestroyQueryPool(device={:#x}, queryPool={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool));
        s.on_vkDestroyQueryPool(device, queryPool, nullptr);
        RVK_TRACE("server vkDestroyQueryPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySamplerYcbcrConversion: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversion(r, ycbcrConversion)) return false;
        RVK_TRACE("server vkDestroySamplerYcbcrConversion(device={:#x}, ycbcrConversion={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(ycbcrConversion));
        s.on_vkDestroySamplerYcbcrConversion(device, ycbcrConversion, nullptr);
        RVK_TRACE("server vkDestroySamplerYcbcrConversion -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroups: {
        ::av::WireArena arena;
        (void)arena;
        VkInstance instance = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pPhysicalDeviceGroupCount_local = 0;
        std::vector<VkPhysicalDeviceGroupProperties> pPhysicalDeviceGroupProperties_storage;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceGroupCount_local)) return false;
            pPhysicalDeviceGroupProperties_storage.resize(pPhysicalDeviceGroupCount_local);
            for (auto& __e : pPhysicalDeviceGroupProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkEnumeratePhysicalDeviceGroups(instance={:#x}, pPhysicalDeviceGroupCount=ptr, pPhysicalDeviceGroupProperties=ptr)",::rvk::instr::trace_handle(instance));
        VkResult result = s.on_vkEnumeratePhysicalDeviceGroups(instance, &pPhysicalDeviceGroupCount_local, array_present ? pPhysicalDeviceGroupProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pPhysicalDeviceGroupCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPhysicalDeviceGroupCount_local; ++k) {
                serialize_VkPhysicalDeviceGroupProperties(reply, pPhysicalDeviceGroupProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkEnumeratePhysicalDeviceGroups -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkFlushMappedMemoryRanges: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t memoryRangeCount{};
        std::vector<VkMappedMemoryRange> pMemoryRanges_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(memoryRangeCount)) return false;
        pMemoryRanges_storage.resize(static_cast<std::size_t>(memoryRangeCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryRangeCount); ++__i) {
            if (!deserialize_VkMappedMemoryRange(r, &pMemoryRanges_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkFlushMappedMemoryRanges(device={:#x}, memoryRangeCount={}, pMemoryRanges=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(memoryRangeCount));
        VkResult result = s.on_vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkFlushMappedMemoryRanges -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferDeviceAddress: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetBufferDeviceAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        VkDeviceAddress result = s.on_vkGetBufferDeviceAddress(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetBufferDeviceAddress -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetBufferMemoryRequirements2(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetBufferMemoryRequirements2(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetBufferMemoryRequirements2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddress: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetBufferOpaqueCaptureAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        uint64_t result = s.on_vkGetBufferOpaqueCaptureAddress(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetBufferOpaqueCaptureAddress -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupport: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayoutSupport pSupport_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pSupport_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSupport_out), arena)) return false;
        RVK_TRACE("server vkGetDescriptorSetLayoutSupport(device={:#x}, pCreateInfo=ptr, pSupport=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDescriptorSetLayoutSupport(device, &pCreateInfo_local, &pSupport_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkDescriptorSetLayoutSupport(reply, pSupport_out);
        RVK_TRACE("server vkGetDescriptorSetLayoutSupport -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceBufferMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceBufferMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceBufferMemoryRequirements(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceBufferMemoryRequirements(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetDeviceBufferMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeatures: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t heapIndex{};
        uint32_t localDeviceIndex{};
        uint32_t remoteDeviceIndex{};
        VkPeerMemoryFeatureFlags pPeerMemoryFeatures_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(heapIndex)) return false;
        if (!r.read_pod(localDeviceIndex)) return false;
        if (!r.read_pod(remoteDeviceIndex)) return false;
        RVK_TRACE("server vkGetDeviceGroupPeerMemoryFeatures(device={:#x}, heapIndex={}, localDeviceIndex={}, remoteDeviceIndex={}, pPeerMemoryFeatures=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(heapIndex), static_cast<long long>(localDeviceIndex), static_cast<long long>(remoteDeviceIndex));
        s.on_vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, &pPeerMemoryFeatures_out);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pPeerMemoryFeatures_out);
        RVK_TRACE("server vkGetDeviceGroupPeerMemoryFeatures -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPresentCapabilitiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceGroupPresentCapabilitiesKHR pDeviceGroupPresentCapabilities_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(pDeviceGroupPresentCapabilities_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pDeviceGroupPresentCapabilities_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceGroupPresentCapabilitiesKHR(device={:#x}, pDeviceGroupPresentCapabilities=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkGetDeviceGroupPresentCapabilitiesKHR(device, &pDeviceGroupPresentCapabilities_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDeviceGroupPresentCapabilitiesKHR(reply, pDeviceGroupPresentCapabilities_out);
        RVK_TRACE("server vkGetDeviceGroupPresentCapabilitiesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupSurfacePresentModesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkDeviceGroupPresentModeFlagsKHR pModes_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSurfaceKHR(r, surface)) return false;
        RVK_TRACE("server vkGetDeviceGroupSurfacePresentModesKHR(device={:#x}, surface={:#x}, pModes=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(surface));
        VkResult result = s.on_vkGetDeviceGroupSurfacePresentModesKHR(device, surface, &pModes_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pModes_out);
        RVK_TRACE("server vkGetDeviceGroupSurfacePresentModesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceImageMemoryRequirements(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceImageMemoryRequirements(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetDeviceImageMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pSparseMemoryRequirementCount_local = 0;
        std::vector<VkSparseImageMemoryRequirements2> pSparseMemoryRequirements_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage.resize(pSparseMemoryRequirementCount_local);
            for (auto& __e : pSparseMemoryRequirements_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetDeviceImageSparseMemoryRequirements(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceImageSparseMemoryRequirements(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pSparseMemoryRequirementCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pSparseMemoryRequirementCount_local; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        RVK_TRACE("server vkGetDeviceImageSparseMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryCommitment: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize pCommittedMemoryInBytes_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        RVK_TRACE("server vkGetDeviceMemoryCommitment(device={:#x}, memory={:#x}, pCommittedMemoryInBytes=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory));
        s.on_vkGetDeviceMemoryCommitment(device, memory, &pCommittedMemoryInBytes_out);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pCommittedMemoryInBytes_out);
        RVK_TRACE("server vkGetDeviceMemoryCommitment -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddress: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemoryOpaqueCaptureAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemoryOpaqueCaptureAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetDeviceMemoryOpaqueCaptureAddress(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        uint64_t result = s.on_vkGetDeviceMemoryOpaqueCaptureAddress(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetDeviceMemoryOpaqueCaptureAddress -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceQueue2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceQueueInfo2 pQueueInfo_local{};
        VkQueue pQueue_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceQueueInfo2(r, &pQueueInfo_local, arena)) return false;
        RVK_TRACE("server vkGetDeviceQueue2(device={:#x}, pQueueInfo=ptr, pQueue=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceQueue2(device, &pQueueInfo_local, &pQueue_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkQueue(reply, pQueue_out);
        RVK_TRACE("server vkGetDeviceQueue2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetImageMemoryRequirements2(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetImageMemoryRequirements2(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetImageMemoryRequirements2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pSparseMemoryRequirementCount_local = 0;
        std::vector<VkSparseImageMemoryRequirements> pSparseMemoryRequirements_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage.resize(pSparseMemoryRequirementCount_local);
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements(device={:#x}, image={:#x}, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkGetImageSparseMemoryRequirements(device, image, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pSparseMemoryRequirementCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pSparseMemoryRequirementCount_local; ++k) {
                serialize_VkSparseImageMemoryRequirements(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageSparseMemoryRequirementsInfo2 pInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pSparseMemoryRequirementCount_local = 0;
        std::vector<VkSparseImageMemoryRequirements2> pSparseMemoryRequirements_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageSparseMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage.resize(pSparseMemoryRequirementCount_local);
            for (auto& __e : pSparseMemoryRequirements_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements2(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetImageSparseMemoryRequirements2(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pSparseMemoryRequirementCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pSparseMemoryRequirementCount_local; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements2 -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource pSubresource_local{};
        VkSubresourceLayout pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!deserialize_VkImageSubresource(r, &pSubresource_local, arena)) return false;
        RVK_TRACE("server vkGetImageSubresourceLayout(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkGetImageSubresourceLayout(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkSubresourceLayout(reply, pLayout_out);
        RVK_TRACE("server vkGetImageSubresourceLayout -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalBufferInfo pExternalBufferInfo_local{};
        VkExternalBufferProperties pExternalBufferProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalBufferInfo(r, &pExternalBufferInfo_local, arena)) return false;
        if (!r.read_pod(pExternalBufferProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalBufferProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalBufferProperties(physicalDevice={:#x}, pExternalBufferInfo=ptr, pExternalBufferProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, &pExternalBufferInfo_local, &pExternalBufferProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalBufferProperties(reply, pExternalBufferProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalBufferProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFenceProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalFenceInfo pExternalFenceInfo_local{};
        VkExternalFenceProperties pExternalFenceProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalFenceInfo(r, &pExternalFenceInfo_local, arena)) return false;
        if (!r.read_pod(pExternalFenceProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalFenceProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalFenceProperties(physicalDevice={:#x}, pExternalFenceInfo=ptr, pExternalFenceProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, &pExternalFenceInfo_local, &pExternalFenceProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalFenceProperties(reply, pExternalFenceProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalFenceProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphoreProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalSemaphoreInfo pExternalSemaphoreInfo_local{};
        VkExternalSemaphoreProperties pExternalSemaphoreProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalSemaphoreInfo(r, &pExternalSemaphoreInfo_local, arena)) return false;
        if (!r.read_pod(pExternalSemaphoreProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalSemaphoreProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice={:#x}, pExternalSemaphoreInfo=ptr, pExternalSemaphoreProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, &pExternalSemaphoreInfo_local, &pExternalSemaphoreProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalSemaphoreProperties(reply, pExternalSemaphoreProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalSemaphoreProperties -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDevicePresentRectanglesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pRectCount_local = 0;
        std::vector<VkRect2D> pRects_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkSurfaceKHR(r, surface)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pRectCount_local)) return false;
            pRects_storage.resize(pRectCount_local);
        }
        RVK_TRACE("server vkGetPhysicalDevicePresentRectanglesKHR(physicalDevice={:#x}, surface={:#x}, pRectCount=ptr, pRects=ptr)",::rvk::instr::trace_handle(physicalDevice), ::rvk::instr::trace_handle(surface));
        VkResult result = s.on_vkGetPhysicalDevicePresentRectanglesKHR(physicalDevice, surface, &pRectCount_local, array_present ? pRects_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pRectCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pRectCount_local; ++k) {
                serialize_VkRect2D(reply, pRects_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDevicePresentRectanglesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceToolProperties: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pToolCount_local = 0;
        std::vector<VkPhysicalDeviceToolProperties> pToolProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pToolCount_local)) return false;
            pToolProperties_storage.resize(pToolCount_local);
            for (auto& __e : pToolProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceToolProperties(physicalDevice={:#x}, pToolCount=ptr, pToolProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkGetPhysicalDeviceToolProperties(physicalDevice, &pToolCount_local, array_present ? pToolProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pToolCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pToolCount_local; ++k) {
                serialize_VkPhysicalDeviceToolProperties(reply, pToolProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceToolProperties -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPrivateData: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkObjectType objectType{};
        uint64_t objectHandle{};
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        uint64_t pData_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(objectType)) return false;
        if (!r.read_pod(objectHandle)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        RVK_TRACE("server vkGetPrivateData(device={:#x}, objectType={}, objectHandle={}, privateDataSlot={:#x}, pData=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(objectType), static_cast<long long>(objectHandle), ::rvk::instr::trace_handle(privateDataSlot));
        s.on_vkGetPrivateData(device, objectType, objectHandle, privateDataSlot, &pData_out);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pData_out);
        RVK_TRACE("server vkGetPrivateData -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetQueryPoolResults: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        size_t dataSize{};
        std::vector<std::uint8_t> pData_storage;
        VkDeviceSize stride{};
        VkQueryResultFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        {
            std::uint64_t __wide = 0;
            if (!r.read_pod(__wide)) return false;
            dataSize = static_cast<size_t>(__wide);
        }
        pData_storage.resize(static_cast<std::size_t>(dataSize));
        if (!r.read_pod(stride)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkGetQueryPoolResults(device={:#x}, queryPool={:#x}, firstQuery={}, queryCount={}, dataSize={}, pData=ptr, stride={}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount), static_cast<long long>(dataSize), static_cast<long long>(stride), static_cast<long long>(flags));
        VkResult result = s.on_vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData_storage.data(), stride, flags);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        if (!pData_storage.empty()) {
            reply.write(pData_storage.data(), pData_storage.size());
        }
        RVK_TRACE("server vkGetQueryPoolResults -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRenderAreaGranularity: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkExtent2D pGranularity_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPass(r, renderPass)) return false;
        RVK_TRACE("server vkGetRenderAreaGranularity(device={:#x}, renderPass={:#x}, pGranularity=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(renderPass));
        s.on_vkGetRenderAreaGranularity(device, renderPass, &pGranularity_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExtent2D(reply, pGranularity_out);
        RVK_TRACE("server vkGetRenderAreaGranularity -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreCounterValue: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        uint64_t pValue_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        RVK_TRACE("server vkGetSemaphoreCounterValue(device={:#x}, semaphore={:#x}, pValue=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
        VkResult result = s.on_vkGetSemaphoreCounterValue(device, semaphore, &pValue_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pValue_out);
        RVK_TRACE("server vkGetSemaphoreCounterValue -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkInvalidateMappedMemoryRanges: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t memoryRangeCount{};
        std::vector<VkMappedMemoryRange> pMemoryRanges_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(memoryRangeCount)) return false;
        pMemoryRanges_storage.resize(static_cast<std::size_t>(memoryRangeCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(memoryRangeCount); ++__i) {
            if (!deserialize_VkMappedMemoryRange(r, &pMemoryRanges_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkInvalidateMappedMemoryRanges(device={:#x}, memoryRangeCount={}, pMemoryRanges=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(memoryRangeCount));
        VkResult result = s.on_vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkInvalidateMappedMemoryRanges -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkMergePipelineCaches: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCache dstCache = VK_NULL_HANDLE;
        uint32_t srcCacheCount{};
        std::vector<VkPipelineCache> pSrcCaches_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCache(r, dstCache)) return false;
        if (!r.read_pod(srcCacheCount)) return false;
        pSrcCaches_storage.resize(static_cast<std::size_t>(srcCacheCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(srcCacheCount); ++__i) {
            if (!deserialize_VkPipelineCache(r, pSrcCaches_storage[__i])) return false;
        }
        RVK_TRACE("server vkMergePipelineCaches(device={:#x}, dstCache={:#x}, srcCacheCount={}, pSrcCaches=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(dstCache), static_cast<long long>(srcCacheCount));
        VkResult result = s.on_vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkMergePipelineCaches -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueBindSparse: {
        ::av::WireArena arena;
        (void)arena;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        std::vector<VkBindSparseInfo> pBindInfo_storage;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfo_storage.resize(static_cast<std::size_t>(bindInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
            if (!deserialize_VkBindSparseInfo(r, &pBindInfo_storage[__i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkQueueBindSparse(queue={:#x}, bindInfoCount={}, pBindInfo=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(bindInfoCount), ::rvk::instr::trace_handle(fence));
        VkResult result = s.on_vkQueueBindSparse(queue, bindInfoCount, pBindInfo_storage.data(), fence);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkQueueBindSparse -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit2: {
        ::av::WireArena arena;
        (void)arena;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        std::vector<VkSubmitInfo2> pSubmits_storage;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage.resize(static_cast<std::size_t>(submitCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
            if (!deserialize_VkSubmitInfo2(r, &pSubmits_storage[__i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkQueueSubmit2(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
        VkResult result = s.on_vkQueueSubmit2(queue, submitCount, pSubmits_storage.data(), fence);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkQueueSubmit2 -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkResetQueryPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        uint32_t queryCount{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(firstQuery)) return false;
        if (!r.read_pod(queryCount)) return false;
        RVK_TRACE("server vkResetQueryPool(device={:#x}, queryPool={:#x}, firstQuery={}, queryCount={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery), static_cast<long long>(queryCount));
        s.on_vkResetQueryPool(device, queryPool, firstQuery, queryCount);
        RVK_TRACE("server vkResetQueryPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkSetPrivateData: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkObjectType objectType{};
        uint64_t objectHandle{};
        VkPrivateDataSlot privateDataSlot = VK_NULL_HANDLE;
        uint64_t data{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(objectType)) return false;
        if (!r.read_pod(objectHandle)) return false;
        if (!deserialize_VkPrivateDataSlot(r, privateDataSlot)) return false;
        if (!r.read_pod(data)) return false;
        RVK_TRACE("server vkSetPrivateData(device={:#x}, objectType={}, objectHandle={}, privateDataSlot={:#x}, data={})",::rvk::instr::trace_handle(device), static_cast<long long>(objectType), static_cast<long long>(objectHandle), ::rvk::instr::trace_handle(privateDataSlot), static_cast<long long>(data));
        VkResult result = s.on_vkSetPrivateData(device, objectType, objectHandle, privateDataSlot, data);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkSetPrivateData -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSignalSemaphore: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreSignalInfo pSignalInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreSignalInfo(r, &pSignalInfo_local, arena)) return false;
        RVK_TRACE("server vkSignalSemaphore(device={:#x}, pSignalInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkSignalSemaphore(device, &pSignalInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkSignalSemaphore -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTrimCommandPool: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolTrimFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkTrimCommandPool(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
        s.on_vkTrimCommandPool(device, commandPool, flags);
        RVK_TRACE("server vkTrimCommandPool -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitSemaphores: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreWaitInfo pWaitInfo_local{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreWaitInfo(r, &pWaitInfo_local, arena)) return false;
        if (!r.read_pod(timeout)) return false;
        RVK_TRACE("server vkWaitSemaphores(device={:#x}, pWaitInfo=ptr, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(timeout));
        VkResult result = s.on_vkWaitSemaphores(device, &pWaitInfo_local, timeout);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkWaitSemaphores -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFeatures2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures2 pFeatures_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pFeatures_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFeatures_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFeatures2KHR(physicalDevice={:#x}, pFeatures=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &pFeatures_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceFeatures2(reply, pFeatures_out);
        RVK_TRACE("server vkGetPhysicalDeviceFeatures2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFormatProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkFormat format{};
        VkFormatProperties2 pFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(format)) return false;
        if (!r.read_pod(pFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pFormatProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties2KHR(physicalDevice={:#x}, format={}, pFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice), static_cast<long long>(format));
        s.on_vkGetPhysicalDeviceFormatProperties2KHR(physicalDevice, format, &pFormatProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkFormatProperties2(reply, pFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceFormatProperties2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceImageFormatProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceImageFormatInfo2 pImageFormatInfo_local{};
        VkImageFormatProperties2 pImageFormatProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceImageFormatInfo2(r, &pImageFormatInfo_local, arena)) return false;
        if (!r.read_pod(pImageFormatProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pImageFormatProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties2KHR(physicalDevice={:#x}, pImageFormatInfo=ptr, pImageFormatProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkGetPhysicalDeviceImageFormatProperties2KHR(physicalDevice, &pImageFormatInfo_local, &pImageFormatProperties_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkImageFormatProperties2(reply, pImageFormatProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceImageFormatProperties2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties2 pMemoryProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pMemoryProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties2KHR(physicalDevice={:#x}, pMemoryProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceMemoryProperties2KHR(physicalDevice, &pMemoryProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceMemoryProperties2(reply, pMemoryProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceMemoryProperties2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 pProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(pProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceProperties2KHR(physicalDevice={:#x}, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceProperties2KHR(physicalDevice, &pProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkPhysicalDeviceProperties2(reply, pProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceProperties2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceQueueFamilyProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pQueueFamilyPropertyCount_local = 0;
        std::vector<VkQueueFamilyProperties2> pQueueFamilyProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pQueueFamilyPropertyCount_local)) return false;
            pQueueFamilyProperties_storage.resize(pQueueFamilyPropertyCount_local);
            for (auto& __e : pQueueFamilyProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice={:#x}, pQueueFamilyPropertyCount=ptr, pQueueFamilyProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, &pQueueFamilyPropertyCount_local, array_present ? pQueueFamilyProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pQueueFamilyPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pQueueFamilyPropertyCount_local; ++k) {
                serialize_VkQueueFamilyProperties2(reply, pQueueFamilyProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceQueueFamilyProperties2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceSparseImageFormatProperties2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceSparseImageFormatInfo2 pFormatInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pPropertyCount_local = 0;
        std::vector<VkSparseImageFormatProperties2> pProperties_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceSparseImageFormatInfo2(r, &pFormatInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPropertyCount_local)) return false;
            pProperties_storage.resize(pPropertyCount_local);
            for (auto& __e : pProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice={:#x}, pFormatInfo=ptr, pPropertyCount=ptr, pProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice, &pFormatInfo_local, &pPropertyCount_local, array_present ? pProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pPropertyCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPropertyCount_local; ++k) {
                serialize_VkSparseImageFormatProperties2(reply, pProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceSparseImageFormatProperties2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkBindBufferMemory2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        std::vector<VkBindBufferMemoryInfo> pBindInfos_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage.resize(static_cast<std::size_t>(bindInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
            if (!deserialize_VkBindBufferMemoryInfo(r, &pBindInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkBindBufferMemory2KHR(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
        VkResult result = s.on_vkBindBufferMemory2KHR(device, bindInfoCount, pBindInfos_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindBufferMemory2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkBindImageMemory2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t bindInfoCount{};
        std::vector<VkBindImageMemoryInfo> pBindInfos_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(bindInfoCount)) return false;
        pBindInfos_storage.resize(static_cast<std::size_t>(bindInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindInfoCount); ++__i) {
            if (!deserialize_VkBindImageMemoryInfo(r, &pBindInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkBindImageMemory2KHR(device={:#x}, bindInfoCount={}, pBindInfos=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(bindInfoCount));
        VkResult result = s.on_vkBindImageMemory2KHR(device, bindInfoCount, pBindInfos_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBindImageMemory2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferMemoryRequirements2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetBufferMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetBufferMemoryRequirements2KHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetBufferMemoryRequirements2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageMemoryRequirements2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageMemoryRequirementsInfo2 pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetImageMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetImageMemoryRequirements2KHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetImageMemoryRequirements2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSparseMemoryRequirements2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImageSparseMemoryRequirementsInfo2 pInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pSparseMemoryRequirementCount_local = 0;
        std::vector<VkSparseImageMemoryRequirements2> pSparseMemoryRequirements_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImageSparseMemoryRequirementsInfo2(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage.resize(pSparseMemoryRequirementCount_local);
            for (auto& __e : pSparseMemoryRequirements_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements2KHR(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetImageSparseMemoryRequirements2KHR(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pSparseMemoryRequirementCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pSparseMemoryRequirementCount_local; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        RVK_TRACE("server vkGetImageSparseMemoryRequirements2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderPass2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo pRenderPassBegin_local{};
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderPassBeginInfo(r, &pRenderPassBegin_local, arena)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBeginRenderPass2KHR(commandBuffer={:#x}, pRenderPassBegin=ptr, pSubpassBeginInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBeginRenderPass2KHR(commandBuffer, &pRenderPassBegin_local, &pSubpassBeginInfo_local);
        RVK_TRACE("server vkCmdBeginRenderPass2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginRenderingKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInfo pRenderingInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInfo(r, &pRenderingInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBeginRenderingKHR(commandBuffer={:#x}, pRenderingInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBeginRenderingKHR(commandBuffer, &pRenderingInfo_local);
        RVK_TRACE("server vkCmdBeginRenderingKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBlitImage2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBlitImageInfo2 pBlitImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBlitImageInfo2(r, &pBlitImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdBlitImage2KHR(commandBuffer={:#x}, pBlitImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBlitImage2KHR(commandBuffer, &pBlitImageInfo_local);
        RVK_TRACE("server vkCmdBlitImage2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBuffer2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferInfo2 pCopyBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferInfo2(r, &pCopyBufferInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyBuffer2KHR(commandBuffer={:#x}, pCopyBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyBuffer2KHR(commandBuffer, &pCopyBufferInfo_local);
        RVK_TRACE("server vkCmdCopyBuffer2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyBufferToImage2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyBufferToImageInfo2 pCopyBufferToImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyBufferToImageInfo2(r, &pCopyBufferToImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyBufferToImage2KHR(commandBuffer={:#x}, pCopyBufferToImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyBufferToImage2KHR(commandBuffer, &pCopyBufferToImageInfo_local);
        RVK_TRACE("server vkCmdCopyBufferToImage2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImage2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageInfo2 pCopyImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageInfo2(r, &pCopyImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyImage2KHR(commandBuffer={:#x}, pCopyImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyImage2KHR(commandBuffer, &pCopyImageInfo_local);
        RVK_TRACE("server vkCmdCopyImage2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyImageToBuffer2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyImageToBufferInfo2 pCopyImageToBufferInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyImageToBufferInfo2(r, &pCopyImageToBufferInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyImageToBuffer2KHR(commandBuffer={:#x}, pCopyImageToBufferInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyImageToBuffer2KHR(commandBuffer, &pCopyImageToBufferInfo_local);
        RVK_TRACE("server vkCmdCopyImageToBuffer2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDispatchBaseKHR: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdDispatchBaseKHR(commandBuffer={:#x}, baseGroupX={}, baseGroupY={}, baseGroupZ={}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(baseGroupX), static_cast<long long>(baseGroupY), static_cast<long long>(baseGroupZ), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
        s.on_vkCmdDispatchBaseKHR(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
        RVK_TRACE("server vkCmdDispatchBaseKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndexedIndirectCountKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndexedIndirectCountKHR(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndexedIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        RVK_TRACE("server vkCmdDrawIndexedIndirectCountKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectCountKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawIndirectCountKHR(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        RVK_TRACE("server vkCmdDrawIndirectCountKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderPass2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdEndRenderPass2KHR(commandBuffer={:#x}, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndRenderPass2KHR(commandBuffer, &pSubpassEndInfo_local);
        RVK_TRACE("server vkCmdEndRenderPass2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndRenderingKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        RVK_TRACE("server vkCmdEndRenderingKHR(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndRenderingKHR(commandBuffer);
        RVK_TRACE("server vkCmdEndRenderingKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdNextSubpass2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSubpassBeginInfo pSubpassBeginInfo_local{};
        VkSubpassEndInfo pSubpassEndInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkSubpassBeginInfo(r, &pSubpassBeginInfo_local, arena)) return false;
        if (!deserialize_VkSubpassEndInfo(r, &pSubpassEndInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdNextSubpass2KHR(commandBuffer={:#x}, pSubpassBeginInfo=ptr, pSubpassEndInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdNextSubpass2KHR(commandBuffer, &pSubpassBeginInfo_local, &pSubpassEndInfo_local);
        RVK_TRACE("server vkCmdNextSubpass2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPipelineBarrier2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdPipelineBarrier2KHR(commandBuffer={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdPipelineBarrier2KHR(commandBuffer, &pDependencyInfo_local);
        RVK_TRACE("server vkCmdPipelineBarrier2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResetEvent2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stageMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!r.read_pod(stageMask)) return false;
        RVK_TRACE("server vkCmdResetEvent2KHR(commandBuffer={:#x}, event={:#x}, stageMask={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event), static_cast<long long>(stageMask));
        s.on_vkCmdResetEvent2KHR(commandBuffer, event, stageMask);
        RVK_TRACE("server vkCmdResetEvent2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdResolveImage2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResolveImageInfo2 pResolveImageInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkResolveImageInfo2(r, &pResolveImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdResolveImage2KHR(commandBuffer={:#x}, pResolveImageInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdResolveImage2KHR(commandBuffer, &pResolveImageInfo_local);
        RVK_TRACE("server vkCmdResolveImage2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDeviceMaskKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t deviceMask{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(deviceMask)) return false;
        RVK_TRACE("server vkCmdSetDeviceMaskKHR(commandBuffer={:#x}, deviceMask={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(deviceMask));
        s.on_vkCmdSetDeviceMaskKHR(commandBuffer, deviceMask);
        RVK_TRACE("server vkCmdSetDeviceMaskKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetEvent2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkEvent event = VK_NULL_HANDLE;
        VkDependencyInfo pDependencyInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkEvent(r, event)) return false;
        if (!deserialize_VkDependencyInfo(r, &pDependencyInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdSetEvent2KHR(commandBuffer={:#x}, event={:#x}, pDependencyInfo=ptr)",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(event));
        s.on_vkCmdSetEvent2KHR(commandBuffer, event, &pDependencyInfo_local);
        RVK_TRACE("server vkCmdSetEvent2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWaitEvents2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t eventCount{};
        std::vector<VkEvent> pEvents_storage;
        std::vector<VkDependencyInfo> pDependencyInfos_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(eventCount)) return false;
        pEvents_storage.resize(static_cast<std::size_t>(eventCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
            if (!deserialize_VkEvent(r, pEvents_storage[__i])) return false;
        }
        pDependencyInfos_storage.resize(static_cast<std::size_t>(eventCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(eventCount); ++__i) {
            if (!deserialize_VkDependencyInfo(r, &pDependencyInfos_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdWaitEvents2KHR(commandBuffer={:#x}, eventCount={}, pEvents=ptr, pDependencyInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(eventCount));
        s.on_vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents_storage.data(), pDependencyInfos_storage.data());
        RVK_TRACE("server vkCmdWaitEvents2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteTimestamp2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineStageFlags2 stage{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stage)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        RVK_TRACE("server vkCmdWriteTimestamp2KHR(commandBuffer={:#x}, stage={}, queryPool={:#x}, query={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stage), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query));
        s.on_vkCmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
        RVK_TRACE("server vkCmdWriteTimestamp2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDescriptorUpdateTemplateKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplateCreateInfo pCreateInfo_local{};
        VkDescriptorUpdateTemplate pDescriptorUpdateTemplate_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplateCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateDescriptorUpdateTemplateKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pDescriptorUpdateTemplate=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateDescriptorUpdateTemplateKHR(device, &pCreateInfo_local, nullptr, &pDescriptorUpdateTemplate_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDescriptorUpdateTemplate(reply, pDescriptorUpdateTemplate_out);
        RVK_TRACE("server vkCreateDescriptorUpdateTemplateKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRenderPass2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPassCreateInfo2 pCreateInfo_local{};
        VkRenderPass pRenderPass_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderPassCreateInfo2(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateRenderPass2KHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pRenderPass=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateRenderPass2KHR(device, &pCreateInfo_local, nullptr, &pRenderPass_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkRenderPass(reply, pRenderPass_out);
        RVK_TRACE("server vkCreateRenderPass2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateSamplerYcbcrConversionKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversionCreateInfo pCreateInfo_local{};
        VkSamplerYcbcrConversion pYcbcrConversion_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversionCreateInfo(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateSamplerYcbcrConversionKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pYcbcrConversion=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateSamplerYcbcrConversionKHR(device, &pCreateInfo_local, nullptr, &pYcbcrConversion_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkSamplerYcbcrConversion(reply, pYcbcrConversion_out);
        RVK_TRACE("server vkCreateSamplerYcbcrConversionKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDescriptorUpdateTemplateKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorUpdateTemplate descriptorUpdateTemplate = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorUpdateTemplate(r, descriptorUpdateTemplate)) return false;
        RVK_TRACE("server vkDestroyDescriptorUpdateTemplateKHR(device={:#x}, descriptorUpdateTemplate={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(descriptorUpdateTemplate));
        s.on_vkDestroyDescriptorUpdateTemplateKHR(device, descriptorUpdateTemplate, nullptr);
        RVK_TRACE("server vkDestroyDescriptorUpdateTemplateKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroySamplerYcbcrConversionKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSamplerYcbcrConversion(r, ycbcrConversion)) return false;
        RVK_TRACE("server vkDestroySamplerYcbcrConversionKHR(device={:#x}, ycbcrConversion={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(ycbcrConversion));
        s.on_vkDestroySamplerYcbcrConversionKHR(device, ycbcrConversion, nullptr);
        RVK_TRACE("server vkDestroySamplerYcbcrConversionKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkEnumeratePhysicalDeviceGroupsKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkInstance instance = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pPhysicalDeviceGroupCount_local = 0;
        std::vector<VkPhysicalDeviceGroupProperties> pPhysicalDeviceGroupProperties_storage;
        if (!deserialize_VkInstance(r, instance)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pPhysicalDeviceGroupCount_local)) return false;
            pPhysicalDeviceGroupProperties_storage.resize(pPhysicalDeviceGroupCount_local);
            for (auto& __e : pPhysicalDeviceGroupProperties_storage) { __e.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkEnumeratePhysicalDeviceGroupsKHR(instance={:#x}, pPhysicalDeviceGroupCount=ptr, pPhysicalDeviceGroupProperties=ptr)",::rvk::instr::trace_handle(instance));
        VkResult result = s.on_vkEnumeratePhysicalDeviceGroupsKHR(instance, &pPhysicalDeviceGroupCount_local, array_present ? pPhysicalDeviceGroupProperties_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pPhysicalDeviceGroupCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pPhysicalDeviceGroupCount_local; ++k) {
                serialize_VkPhysicalDeviceGroupProperties(reply, pPhysicalDeviceGroupProperties_storage[k]);
            }
        }
        RVK_TRACE("server vkEnumeratePhysicalDeviceGroupsKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferDeviceAddressKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetBufferDeviceAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        VkDeviceAddress result = s.on_vkGetBufferDeviceAddressKHR(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetBufferDeviceAddressKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetBufferOpaqueCaptureAddressKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkBufferDeviceAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkBufferDeviceAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetBufferOpaqueCaptureAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        uint64_t result = s.on_vkGetBufferOpaqueCaptureAddressKHR(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetBufferOpaqueCaptureAddressKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDescriptorSetLayoutSupportKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo pCreateInfo_local{};
        VkDescriptorSetLayoutSupport pSupport_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDescriptorSetLayoutCreateInfo(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pSupport_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSupport_out), arena)) return false;
        RVK_TRACE("server vkGetDescriptorSetLayoutSupportKHR(device={:#x}, pCreateInfo=ptr, pSupport=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDescriptorSetLayoutSupportKHR(device, &pCreateInfo_local, &pSupport_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkDescriptorSetLayoutSupport(reply, pSupport_out);
        RVK_TRACE("server vkGetDescriptorSetLayoutSupportKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceBufferMemoryRequirementsKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceBufferMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceBufferMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceBufferMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceBufferMemoryRequirementsKHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetDeviceBufferMemoryRequirementsKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceGroupPeerMemoryFeaturesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t heapIndex{};
        uint32_t localDeviceIndex{};
        uint32_t remoteDeviceIndex{};
        VkPeerMemoryFeatureFlags pPeerMemoryFeatures_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(heapIndex)) return false;
        if (!r.read_pod(localDeviceIndex)) return false;
        if (!r.read_pod(remoteDeviceIndex)) return false;
        RVK_TRACE("server vkGetDeviceGroupPeerMemoryFeaturesKHR(device={:#x}, heapIndex={}, localDeviceIndex={}, remoteDeviceIndex={}, pPeerMemoryFeatures=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(heapIndex), static_cast<long long>(localDeviceIndex), static_cast<long long>(remoteDeviceIndex));
        s.on_vkGetDeviceGroupPeerMemoryFeaturesKHR(device, heapIndex, localDeviceIndex, remoteDeviceIndex, &pPeerMemoryFeatures_out);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pPeerMemoryFeatures_out);
        RVK_TRACE("server vkGetDeviceGroupPeerMemoryFeaturesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageMemoryRequirementsKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        VkMemoryRequirements2 pMemoryRequirements_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pMemoryRequirements_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryRequirements_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceImageMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceImageMemoryRequirementsKHR(device, &pInfo_local, &pMemoryRequirements_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkMemoryRequirements2(reply, pMemoryRequirements_out);
        RVK_TRACE("server vkGetDeviceImageMemoryRequirementsKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSparseMemoryRequirementsKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageMemoryRequirements pInfo_local{};
        std::uint8_t array_present = 0;
        std::uint32_t pSparseMemoryRequirementCount_local = 0;
        std::vector<VkSparseImageMemoryRequirements2> pSparseMemoryRequirements_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageMemoryRequirements(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pSparseMemoryRequirementCount_local)) return false;
            pSparseMemoryRequirements_storage.resize(pSparseMemoryRequirementCount_local);
            for (auto& __e : pSparseMemoryRequirements_storage) { __e.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetDeviceImageSparseMemoryRequirementsKHR(device={:#x}, pInfo=ptr, pSparseMemoryRequirementCount=ptr, pSparseMemoryRequirements=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceImageSparseMemoryRequirementsKHR(device, &pInfo_local, &pSparseMemoryRequirementCount_local, array_present ? pSparseMemoryRequirements_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pSparseMemoryRequirementCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pSparseMemoryRequirementCount_local; ++k) {
                serialize_VkSparseImageMemoryRequirements2(reply, pSparseMemoryRequirements_storage[k]);
            }
        }
        RVK_TRACE("server vkGetDeviceImageSparseMemoryRequirementsKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceMemoryOpaqueCaptureAddressKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemoryOpaqueCaptureAddressInfo pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemoryOpaqueCaptureAddressInfo(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetDeviceMemoryOpaqueCaptureAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        uint64_t result = s.on_vkGetDeviceMemoryOpaqueCaptureAddressKHR(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetDeviceMemoryOpaqueCaptureAddressKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalBufferPropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalBufferInfo pExternalBufferInfo_local{};
        VkExternalBufferProperties pExternalBufferProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalBufferInfo(r, &pExternalBufferInfo_local, arena)) return false;
        if (!r.read_pod(pExternalBufferProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalBufferProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice={:#x}, pExternalBufferInfo=ptr, pExternalBufferProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice, &pExternalBufferInfo_local, &pExternalBufferProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalBufferProperties(reply, pExternalBufferProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalBufferPropertiesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalFencePropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalFenceInfo pExternalFenceInfo_local{};
        VkExternalFenceProperties pExternalFenceProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalFenceInfo(r, &pExternalFenceInfo_local, arena)) return false;
        if (!r.read_pod(pExternalFenceProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalFenceProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalFencePropertiesKHR(physicalDevice={:#x}, pExternalFenceInfo=ptr, pExternalFenceProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalFencePropertiesKHR(physicalDevice, &pExternalFenceInfo_local, &pExternalFenceProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalFenceProperties(reply, pExternalFenceProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalFencePropertiesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceExternalSemaphoreInfo pExternalSemaphoreInfo_local{};
        VkExternalSemaphoreProperties pExternalSemaphoreProperties_out{};
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!deserialize_VkPhysicalDeviceExternalSemaphoreInfo(r, &pExternalSemaphoreInfo_local, arena)) return false;
        if (!r.read_pod(pExternalSemaphoreProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pExternalSemaphoreProperties_out), arena)) return false;
        RVK_TRACE("server vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice={:#x}, pExternalSemaphoreInfo=ptr, pExternalSemaphoreProperties=ptr)",::rvk::instr::trace_handle(physicalDevice));
        s.on_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice, &pExternalSemaphoreInfo_local, &pExternalSemaphoreProperties_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExternalSemaphoreProperties(reply, pExternalSemaphoreProperties_out);
        RVK_TRACE("server vkGetPhysicalDeviceExternalSemaphorePropertiesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreCounterValueKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        uint64_t pValue_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphore(r, semaphore)) return false;
        RVK_TRACE("server vkGetSemaphoreCounterValueKHR(device={:#x}, semaphore={:#x}, pValue=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(semaphore));
        VkResult result = s.on_vkGetSemaphoreCounterValueKHR(device, semaphore, &pValue_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pValue_out);
        RVK_TRACE("server vkGetSemaphoreCounterValueKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkQueueSubmit2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t submitCount{};
        std::vector<VkSubmitInfo2> pSubmits_storage;
        VkFence fence = VK_NULL_HANDLE;
        if (!deserialize_VkQueue(r, queue)) return false;
        if (!r.read_pod(submitCount)) return false;
        pSubmits_storage.resize(static_cast<std::size_t>(submitCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(submitCount); ++__i) {
            if (!deserialize_VkSubmitInfo2(r, &pSubmits_storage[__i], arena)) return false;
        }
        if (!deserialize_VkFence(r, fence)) return false;
        RVK_TRACE("server vkQueueSubmit2KHR(queue={:#x}, submitCount={}, pSubmits=ptr, fence={:#x})",::rvk::instr::trace_handle(queue), static_cast<long long>(submitCount), ::rvk::instr::trace_handle(fence));
        VkResult result = s.on_vkQueueSubmit2KHR(queue, submitCount, pSubmits_storage.data(), fence);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkQueueSubmit2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkSignalSemaphoreKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreSignalInfo pSignalInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreSignalInfo(r, &pSignalInfo_local, arena)) return false;
        RVK_TRACE("server vkSignalSemaphoreKHR(device={:#x}, pSignalInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkSignalSemaphoreKHR(device, &pSignalInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkSignalSemaphoreKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTrimCommandPoolKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandPoolTrimFlags flags{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCommandPool(r, commandPool)) return false;
        if (!r.read_pod(flags)) return false;
        RVK_TRACE("server vkTrimCommandPoolKHR(device={:#x}, commandPool={:#x}, flags={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(commandPool), static_cast<long long>(flags));
        s.on_vkTrimCommandPoolKHR(device, commandPool, flags);
        RVK_TRACE("server vkTrimCommandPoolKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkWaitSemaphoresKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreWaitInfo pWaitInfo_local{};
        uint64_t timeout{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreWaitInfo(r, &pWaitInfo_local, arena)) return false;
        if (!r.read_pod(timeout)) return false;
        RVK_TRACE("server vkWaitSemaphoresKHR(device={:#x}, pWaitInfo=ptr, timeout={})",::rvk::instr::trace_handle(device), static_cast<long long>(timeout));
        VkResult result = s.on_vkWaitSemaphoresKHR(device, &pWaitInfo_local, timeout);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkWaitSemaphoresKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginConditionalRenderingEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkConditionalRenderingBeginInfoEXT pConditionalRenderingBegin_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkConditionalRenderingBeginInfoEXT(r, &pConditionalRenderingBegin_local, arena)) return false;
        RVK_TRACE("server vkCmdBeginConditionalRenderingEXT(commandBuffer={:#x}, pConditionalRenderingBegin=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdBeginConditionalRenderingEXT(commandBuffer, &pConditionalRenderingBegin_local);
        RVK_TRACE("server vkCmdBeginConditionalRenderingEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndConditionalRenderingEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        RVK_TRACE("server vkCmdEndConditionalRenderingEXT(commandBuffer={:#x})",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdEndConditionalRenderingEXT(commandBuffer);
        RVK_TRACE("server vkCmdEndConditionalRenderingEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorWriteEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t attachmentCount{};
        std::vector<VkBool32> pColorWriteEnables_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorWriteEnables_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!r.read_pod(pColorWriteEnables_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetColorWriteEnableEXT(commandBuffer={:#x}, attachmentCount={}, pColorWriteEnables=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(attachmentCount));
        s.on_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables_storage.data());
        RVK_TRACE("server vkCmdSetColorWriteEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBiasEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBiasEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBiasEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthBiasEnableEXT(commandBuffer={:#x}, depthBiasEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBiasEnable));
        s.on_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
        RVK_TRACE("server vkCmdSetDepthBiasEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLogicOpEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkLogicOp logicOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(logicOp)) return false;
        RVK_TRACE("server vkCmdSetLogicOpEXT(commandBuffer={:#x}, logicOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(logicOp));
        s.on_vkCmdSetLogicOpEXT(commandBuffer, logicOp);
        RVK_TRACE("server vkCmdSetLogicOpEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPatchControlPointsEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t patchControlPoints{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(patchControlPoints)) return false;
        RVK_TRACE("server vkCmdSetPatchControlPointsEXT(commandBuffer={:#x}, patchControlPoints={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(patchControlPoints));
        s.on_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
        RVK_TRACE("server vkCmdSetPatchControlPointsEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveRestartEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 primitiveRestartEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveRestartEnable)) return false;
        RVK_TRACE("server vkCmdSetPrimitiveRestartEnableEXT(commandBuffer={:#x}, primitiveRestartEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveRestartEnable));
        s.on_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
        RVK_TRACE("server vkCmdSetPrimitiveRestartEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizerDiscardEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 rasterizerDiscardEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizerDiscardEnable)) return false;
        RVK_TRACE("server vkCmdSetRasterizerDiscardEnableEXT(commandBuffer={:#x}, rasterizerDiscardEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizerDiscardEnable));
        s.on_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
        RVK_TRACE("server vkCmdSetRasterizerDiscardEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetVertexInputEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t vertexBindingDescriptionCount{};
        std::vector<VkVertexInputBindingDescription2EXT> pVertexBindingDescriptions_storage;
        uint32_t vertexAttributeDescriptionCount{};
        std::vector<VkVertexInputAttributeDescription2EXT> pVertexAttributeDescriptions_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(vertexBindingDescriptionCount)) return false;
        pVertexBindingDescriptions_storage.resize(static_cast<std::size_t>(vertexBindingDescriptionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(vertexBindingDescriptionCount); ++__i) {
            if (!deserialize_VkVertexInputBindingDescription2EXT(r, &pVertexBindingDescriptions_storage[__i], arena)) return false;
        }
        if (!r.read_pod(vertexAttributeDescriptionCount)) return false;
        pVertexAttributeDescriptions_storage.resize(static_cast<std::size_t>(vertexAttributeDescriptionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(vertexAttributeDescriptionCount); ++__i) {
            if (!deserialize_VkVertexInputAttributeDescription2EXT(r, &pVertexAttributeDescriptions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetVertexInputEXT(commandBuffer={:#x}, vertexBindingDescriptionCount={}, pVertexBindingDescriptions=ptr, vertexAttributeDescriptionCount={}, pVertexAttributeDescriptions=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(vertexBindingDescriptionCount), static_cast<long long>(vertexAttributeDescriptionCount));
        s.on_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions_storage.data(), vertexAttributeDescriptionCount, pVertexAttributeDescriptions_storage.data());
        RVK_TRACE("server vkCmdSetVertexInputEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkBuildAccelerationStructuresKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        uint32_t infoCount{};
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> pInfos_storage;
        const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos_outer = nullptr;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage.resize(static_cast<std::size_t>(infoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[__i], arena)) return false;
        }
        {
            const std::uint32_t __n_outer = static_cast<std::uint32_t>(infoCount);
            auto** __ptrs = arena.allocate<const VkAccelerationStructureBuildRangeInfoKHR*>(__n_outer);
            if (__ptrs == nullptr) { return false; }
            for (std::uint32_t __i = 0; __i < __n_outer; ++__i) {
                const std::uint32_t __n = static_cast<std::uint32_t>(pInfos_storage[__i].geometryCount);
                auto* __inner = arena.allocate<VkAccelerationStructureBuildRangeInfoKHR>(__n);
                if (__inner == nullptr) { return false; }
                for (std::uint32_t __j = 0; __j < __n; ++__j) {
                    if (!deserialize_VkAccelerationStructureBuildRangeInfoKHR(r, &__inner[__j], arena)) return false;
                }
                __ptrs[__i] = __inner;
            }
            ppBuildRangeInfos_outer = __ptrs;
        }
        RVK_TRACE("server vkBuildAccelerationStructuresKHR(device={:#x}, deferredOperation={:#x}, infoCount={}, pInfos=ptr, ppBuildRangeInfos=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation), static_cast<long long>(infoCount));
        VkResult result = s.on_vkBuildAccelerationStructuresKHR(device, deferredOperation, infoCount, pInfos_storage.data(), ppBuildRangeInfos_outer);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkBuildAccelerationStructuresKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindShadersEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t stageCount{};
        std::vector<VkShaderStageFlagBits> pStages_storage;
        std::vector<VkShaderEXT> pShaders_storage;
        std::uint8_t pShaders_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stageCount)) return false;
        pStages_storage.resize(static_cast<std::size_t>(stageCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(stageCount); ++__i) {
            if (!r.read_pod(pStages_storage[__i])) return false;
        }
        if (!r.read_pod(pShaders_present)) return false;
        if (pShaders_present) {
            pShaders_storage.resize(static_cast<std::size_t>(stageCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(stageCount); ++__i) {
                if (!deserialize_VkShaderEXT(r, pShaders_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdBindShadersEXT(commandBuffer={:#x}, stageCount={}, pStages=ptr, pShaders=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stageCount));
        s.on_vkCmdBindShadersEXT(commandBuffer, stageCount, pStages_storage.data(), pShaders_present ? pShaders_storage.data() : nullptr);
        RVK_TRACE("server vkCmdBindShadersEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindVertexBuffers2EXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        std::vector<VkBuffer> pBuffers_storage;
        std::vector<VkDeviceSize> pOffsets_storage;
        std::vector<VkDeviceSize> pSizes_storage;
        std::uint8_t pSizes_present = 0;
        std::vector<VkDeviceSize> pStrides_storage;
        std::uint8_t pStrides_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[__i])) return false;
        }
        pOffsets_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!r.read_pod(pOffsets_storage[__i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage.resize(static_cast<std::size_t>(bindingCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
                if (!r.read_pod(pSizes_storage[__i])) return false;
            }
        }
        if (!r.read_pod(pStrides_present)) return false;
        if (pStrides_present) {
            pStrides_storage.resize(static_cast<std::size_t>(bindingCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
                if (!r.read_pod(pStrides_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdBindVertexBuffers2EXT(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr, pStrides=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
        s.on_vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers_storage.data(), pOffsets_storage.data(), pSizes_present ? pSizes_storage.data() : nullptr, pStrides_present ? pStrides_storage.data() : nullptr);
        RVK_TRACE("server vkCmdBindVertexBuffers2EXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresIndirectKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t infoCount{};
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> pInfos_storage;
        std::vector<VkDeviceAddress> pIndirectDeviceAddresses_storage;
        std::vector<uint32_t> pIndirectStrides_storage;
        const uint32_t** ppMaxPrimitiveCounts_outer = nullptr;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage.resize(static_cast<std::size_t>(infoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[__i], arena)) return false;
        }
        pIndirectDeviceAddresses_storage.resize(static_cast<std::size_t>(infoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
            if (!r.read_pod(pIndirectDeviceAddresses_storage[__i])) return false;
        }
        pIndirectStrides_storage.resize(static_cast<std::size_t>(infoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
            if (!r.read_pod(pIndirectStrides_storage[__i])) return false;
        }
        {
            const std::uint32_t __n_outer = static_cast<std::uint32_t>(infoCount);
            auto** __ptrs = arena.allocate<const uint32_t*>(__n_outer);
            if (__ptrs == nullptr) { return false; }
            for (std::uint32_t __i = 0; __i < __n_outer; ++__i) {
                const std::uint32_t __n = static_cast<std::uint32_t>(pInfos_storage[__i].geometryCount);
                auto* __inner = arena.allocate<uint32_t>(__n);
                if (__inner == nullptr) { return false; }
                for (std::uint32_t __j = 0; __j < __n; ++__j) {
                    if (!r.read_pod(__inner[__j])) return false;
                }
                __ptrs[__i] = __inner;
            }
            ppMaxPrimitiveCounts_outer = __ptrs;
        }
        RVK_TRACE("server vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer={:#x}, infoCount={}, pInfos=ptr, pIndirectDeviceAddresses=ptr, pIndirectStrides=ptr, ppMaxPrimitiveCounts=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(infoCount));
        s.on_vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos_storage.data(), pIndirectDeviceAddresses_storage.data(), pIndirectStrides_storage.data(), ppMaxPrimitiveCounts_outer);
        RVK_TRACE("server vkCmdBuildAccelerationStructuresIndirectKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBuildAccelerationStructuresKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t infoCount{};
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> pInfos_storage;
        const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos_outer = nullptr;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(infoCount)) return false;
        pInfos_storage.resize(static_cast<std::size_t>(infoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(infoCount); ++__i) {
            if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pInfos_storage[__i], arena)) return false;
        }
        {
            const std::uint32_t __n_outer = static_cast<std::uint32_t>(infoCount);
            auto** __ptrs = arena.allocate<const VkAccelerationStructureBuildRangeInfoKHR*>(__n_outer);
            if (__ptrs == nullptr) { return false; }
            for (std::uint32_t __i = 0; __i < __n_outer; ++__i) {
                const std::uint32_t __n = static_cast<std::uint32_t>(pInfos_storage[__i].geometryCount);
                auto* __inner = arena.allocate<VkAccelerationStructureBuildRangeInfoKHR>(__n);
                if (__inner == nullptr) { return false; }
                for (std::uint32_t __j = 0; __j < __n; ++__j) {
                    if (!deserialize_VkAccelerationStructureBuildRangeInfoKHR(r, &__inner[__j], arena)) return false;
                }
                __ptrs[__i] = __inner;
            }
            ppBuildRangeInfos_outer = __ptrs;
        }
        RVK_TRACE("server vkCmdBuildAccelerationStructuresKHR(commandBuffer={:#x}, infoCount={}, pInfos=ptr, ppBuildRangeInfos=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(infoCount));
        s.on_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos_storage.data(), ppBuildRangeInfos_outer);
        RVK_TRACE("server vkCmdBuildAccelerationStructuresKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyAccelerationStructureKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyAccelerationStructureKHR(commandBuffer, &pInfo_local);
        RVK_TRACE("server vkCmdCopyAccelerationStructureKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyAccelerationStructureToMemoryKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyAccelerationStructureToMemoryInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyAccelerationStructureToMemoryInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer, &pInfo_local);
        RVK_TRACE("server vkCmdCopyAccelerationStructureToMemoryKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdCopyMemoryToAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCopyMemoryToAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkCopyMemoryToAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer, &pInfo_local);
        RVK_TRACE("server vkCmdCopyMemoryToAccelerationStructureKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t groupCountX{};
        uint32_t groupCountY{};
        uint32_t groupCountZ{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(groupCountX)) return false;
        if (!r.read_pod(groupCountY)) return false;
        if (!r.read_pod(groupCountZ)) return false;
        RVK_TRACE("server vkCmdDrawMeshTasksEXT(commandBuffer={:#x}, groupCountX={}, groupCountY={}, groupCountZ={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(groupCountX), static_cast<long long>(groupCountY), static_cast<long long>(groupCountZ));
        s.on_vkCmdDrawMeshTasksEXT(commandBuffer, groupCountX, groupCountY, groupCountZ);
        RVK_TRACE("server vkCmdDrawMeshTasksEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectCountEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        VkBuffer countBuffer = VK_NULL_HANDLE;
        VkDeviceSize countBufferOffset{};
        uint32_t maxDrawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!deserialize_VkBuffer(r, countBuffer)) return false;
        if (!r.read_pod(countBufferOffset)) return false;
        if (!r.read_pod(maxDrawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer={:#x}, buffer={:#x}, offset={}, countBuffer={:#x}, countBufferOffset={}, maxDrawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), ::rvk::instr::trace_handle(countBuffer), static_cast<long long>(countBufferOffset), static_cast<long long>(maxDrawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        RVK_TRACE("server vkCmdDrawMeshTasksIndirectCountEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawMeshTasksIndirectEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset{};
        uint32_t drawCount{};
        uint32_t stride{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkBuffer(r, buffer)) return false;
        if (!r.read_pod(offset)) return false;
        if (!r.read_pod(drawCount)) return false;
        if (!r.read_pod(stride)) return false;
        RVK_TRACE("server vkCmdDrawMeshTasksIndirectEXT(commandBuffer={:#x}, buffer={:#x}, offset={}, drawCount={}, stride={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(drawCount), static_cast<long long>(stride));
        s.on_vkCmdDrawMeshTasksIndirectEXT(commandBuffer, buffer, offset, drawCount, stride);
        RVK_TRACE("server vkCmdDrawMeshTasksIndirectEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdPushDescriptorSetKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkPipelineLayout layout = VK_NULL_HANDLE;
        uint32_t set{};
        uint32_t descriptorWriteCount{};
        std::vector<VkWriteDescriptorSet> pDescriptorWrites_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineBindPoint)) return false;
        if (!deserialize_VkPipelineLayout(r, layout)) return false;
        if (!r.read_pod(set)) return false;
        if (!r.read_pod(descriptorWriteCount)) return false;
        pDescriptorWrites_storage.resize(static_cast<std::size_t>(descriptorWriteCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(descriptorWriteCount); ++__i) {
            if (!deserialize_VkWriteDescriptorSet(r, &pDescriptorWrites_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdPushDescriptorSetKHR(commandBuffer={:#x}, pipelineBindPoint={}, layout={:#x}, set={}, descriptorWriteCount={}, pDescriptorWrites=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineBindPoint), ::rvk::instr::trace_handle(layout), static_cast<long long>(set), static_cast<long long>(descriptorWriteCount));
        s.on_vkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites_storage.data());
        RVK_TRACE("server vkCmdPushDescriptorSetKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetAlphaToCoverageEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 alphaToCoverageEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(alphaToCoverageEnable)) return false;
        RVK_TRACE("server vkCmdSetAlphaToCoverageEnableEXT(commandBuffer={:#x}, alphaToCoverageEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(alphaToCoverageEnable));
        s.on_vkCmdSetAlphaToCoverageEnableEXT(commandBuffer, alphaToCoverageEnable);
        RVK_TRACE("server vkCmdSetAlphaToCoverageEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetAlphaToOneEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 alphaToOneEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(alphaToOneEnable)) return false;
        RVK_TRACE("server vkCmdSetAlphaToOneEnableEXT(commandBuffer={:#x}, alphaToOneEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(alphaToOneEnable));
        s.on_vkCmdSetAlphaToOneEnableEXT(commandBuffer, alphaToOneEnable);
        RVK_TRACE("server vkCmdSetAlphaToOneEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendAdvancedEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        std::vector<VkColorBlendAdvancedEXT> pColorBlendAdvanced_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendAdvanced_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!deserialize_VkColorBlendAdvancedEXT(r, &pColorBlendAdvanced_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetColorBlendAdvancedEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendAdvanced=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
        s.on_vkCmdSetColorBlendAdvancedEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced_storage.data());
        RVK_TRACE("server vkCmdSetColorBlendAdvancedEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        std::vector<VkBool32> pColorBlendEnables_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendEnables_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!r.read_pod(pColorBlendEnables_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetColorBlendEnableEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendEnables=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
        s.on_vkCmdSetColorBlendEnableEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables_storage.data());
        RVK_TRACE("server vkCmdSetColorBlendEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorBlendEquationEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        std::vector<VkColorBlendEquationEXT> pColorBlendEquations_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorBlendEquations_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!deserialize_VkColorBlendEquationEXT(r, &pColorBlendEquations_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetColorBlendEquationEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorBlendEquations=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
        s.on_vkCmdSetColorBlendEquationEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations_storage.data());
        RVK_TRACE("server vkCmdSetColorBlendEquationEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetColorWriteMaskEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstAttachment{};
        uint32_t attachmentCount{};
        std::vector<VkColorComponentFlags> pColorWriteMasks_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstAttachment)) return false;
        if (!r.read_pod(attachmentCount)) return false;
        pColorWriteMasks_storage.resize(static_cast<std::size_t>(attachmentCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(attachmentCount); ++__i) {
            if (!r.read_pod(pColorWriteMasks_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetColorWriteMaskEXT(commandBuffer={:#x}, firstAttachment={}, attachmentCount={}, pColorWriteMasks=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstAttachment), static_cast<long long>(attachmentCount));
        s.on_vkCmdSetColorWriteMaskEXT(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks_storage.data());
        RVK_TRACE("server vkCmdSetColorWriteMaskEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetConservativeRasterizationModeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkConservativeRasterizationModeEXT conservativeRasterizationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(conservativeRasterizationMode)) return false;
        RVK_TRACE("server vkCmdSetConservativeRasterizationModeEXT(commandBuffer={:#x}, conservativeRasterizationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(conservativeRasterizationMode));
        s.on_vkCmdSetConservativeRasterizationModeEXT(commandBuffer, conservativeRasterizationMode);
        RVK_TRACE("server vkCmdSetConservativeRasterizationModeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationModeNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCoverageModulationModeNV coverageModulationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationMode)) return false;
        RVK_TRACE("server vkCmdSetCoverageModulationModeNV(commandBuffer={:#x}, coverageModulationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationMode));
        s.on_vkCmdSetCoverageModulationModeNV(commandBuffer, coverageModulationMode);
        RVK_TRACE("server vkCmdSetCoverageModulationModeNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationTableEnableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 coverageModulationTableEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationTableEnable)) return false;
        RVK_TRACE("server vkCmdSetCoverageModulationTableEnableNV(commandBuffer={:#x}, coverageModulationTableEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationTableEnable));
        s.on_vkCmdSetCoverageModulationTableEnableNV(commandBuffer, coverageModulationTableEnable);
        RVK_TRACE("server vkCmdSetCoverageModulationTableEnableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageModulationTableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t coverageModulationTableCount{};
        std::vector<float> pCoverageModulationTable_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageModulationTableCount)) return false;
        pCoverageModulationTable_storage.resize(static_cast<std::size_t>(coverageModulationTableCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(coverageModulationTableCount); ++__i) {
            if (!r.read_pod(pCoverageModulationTable_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetCoverageModulationTableNV(commandBuffer={:#x}, coverageModulationTableCount={}, pCoverageModulationTable=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageModulationTableCount));
        s.on_vkCmdSetCoverageModulationTableNV(commandBuffer, coverageModulationTableCount, pCoverageModulationTable_storage.data());
        RVK_TRACE("server vkCmdSetCoverageModulationTableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageReductionModeNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCoverageReductionModeNV coverageReductionMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageReductionMode)) return false;
        RVK_TRACE("server vkCmdSetCoverageReductionModeNV(commandBuffer={:#x}, coverageReductionMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageReductionMode));
        s.on_vkCmdSetCoverageReductionModeNV(commandBuffer, coverageReductionMode);
        RVK_TRACE("server vkCmdSetCoverageReductionModeNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageToColorEnableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 coverageToColorEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageToColorEnable)) return false;
        RVK_TRACE("server vkCmdSetCoverageToColorEnableNV(commandBuffer={:#x}, coverageToColorEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageToColorEnable));
        s.on_vkCmdSetCoverageToColorEnableNV(commandBuffer, coverageToColorEnable);
        RVK_TRACE("server vkCmdSetCoverageToColorEnableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCoverageToColorLocationNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t coverageToColorLocation{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(coverageToColorLocation)) return false;
        RVK_TRACE("server vkCmdSetCoverageToColorLocationNV(commandBuffer={:#x}, coverageToColorLocation={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(coverageToColorLocation));
        s.on_vkCmdSetCoverageToColorLocationNV(commandBuffer, coverageToColorLocation);
        RVK_TRACE("server vkCmdSetCoverageToColorLocationNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetCullModeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCullModeFlags cullMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(cullMode)) return false;
        RVK_TRACE("server vkCmdSetCullModeEXT(commandBuffer={:#x}, cullMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(cullMode));
        s.on_vkCmdSetCullModeEXT(commandBuffer, cullMode);
        RVK_TRACE("server vkCmdSetCullModeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthBoundsTestEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthBoundsTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthBoundsTestEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthBoundsTestEnableEXT(commandBuffer={:#x}, depthBoundsTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthBoundsTestEnable));
        s.on_vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
        RVK_TRACE("server vkCmdSetDepthBoundsTestEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClampEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthClampEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClampEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthClampEnableEXT(commandBuffer={:#x}, depthClampEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClampEnable));
        s.on_vkCmdSetDepthClampEnableEXT(commandBuffer, depthClampEnable);
        RVK_TRACE("server vkCmdSetDepthClampEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClampRangeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkDepthClampModeEXT depthClampMode{};
        VkDepthClampRangeEXT pDepthClampRange_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClampMode)) return false;
        if (!deserialize_VkDepthClampRangeEXT(r, &pDepthClampRange_local, arena)) return false;
        RVK_TRACE("server vkCmdSetDepthClampRangeEXT(commandBuffer={:#x}, depthClampMode={}, pDepthClampRange=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClampMode));
        s.on_vkCmdSetDepthClampRangeEXT(commandBuffer, depthClampMode, &pDepthClampRange_local);
        RVK_TRACE("server vkCmdSetDepthClampRangeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClipEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthClipEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthClipEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthClipEnableEXT(commandBuffer={:#x}, depthClipEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthClipEnable));
        s.on_vkCmdSetDepthClipEnableEXT(commandBuffer, depthClipEnable);
        RVK_TRACE("server vkCmdSetDepthClipEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthClipNegativeOneToOneEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 negativeOneToOne{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(negativeOneToOne)) return false;
        RVK_TRACE("server vkCmdSetDepthClipNegativeOneToOneEXT(commandBuffer={:#x}, negativeOneToOne={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(negativeOneToOne));
        s.on_vkCmdSetDepthClipNegativeOneToOneEXT(commandBuffer, negativeOneToOne);
        RVK_TRACE("server vkCmdSetDepthClipNegativeOneToOneEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthCompareOpEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCompareOp depthCompareOp{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthCompareOp)) return false;
        RVK_TRACE("server vkCmdSetDepthCompareOpEXT(commandBuffer={:#x}, depthCompareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthCompareOp));
        s.on_vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
        RVK_TRACE("server vkCmdSetDepthCompareOpEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthTestEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthTestEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthTestEnableEXT(commandBuffer={:#x}, depthTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthTestEnable));
        s.on_vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
        RVK_TRACE("server vkCmdSetDepthTestEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetDepthWriteEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 depthWriteEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(depthWriteEnable)) return false;
        RVK_TRACE("server vkCmdSetDepthWriteEnableEXT(commandBuffer={:#x}, depthWriteEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(depthWriteEnable));
        s.on_vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
        RVK_TRACE("server vkCmdSetDepthWriteEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetExtraPrimitiveOverestimationSizeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        float extraPrimitiveOverestimationSize{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(extraPrimitiveOverestimationSize)) return false;
        RVK_TRACE("server vkCmdSetExtraPrimitiveOverestimationSizeEXT(commandBuffer={:#x}, extraPrimitiveOverestimationSize={})",::rvk::instr::trace_handle(commandBuffer), extraPrimitiveOverestimationSize);
        s.on_vkCmdSetExtraPrimitiveOverestimationSizeEXT(commandBuffer, extraPrimitiveOverestimationSize);
        RVK_TRACE("server vkCmdSetExtraPrimitiveOverestimationSizeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFragmentShadingRateKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkExtent2D pFragmentSize_local{};
        VkFragmentShadingRateCombinerOpKHR combinerOps_local[2]{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkExtent2D(r, &pFragmentSize_local, arena)) return false;
        for (std::uint32_t __i = 0; __i < 2u; ++__i) {
            if (!r.read_pod(combinerOps_local[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetFragmentShadingRateKHR(commandBuffer={:#x}, pFragmentSize=ptr, combinerOps=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdSetFragmentShadingRateKHR(commandBuffer, &pFragmentSize_local, combinerOps_local);
        RVK_TRACE("server vkCmdSetFragmentShadingRateKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetFrontFaceEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFrontFace frontFace{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(frontFace)) return false;
        RVK_TRACE("server vkCmdSetFrontFaceEXT(commandBuffer={:#x}, frontFace={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(frontFace));
        s.on_vkCmdSetFrontFaceEXT(commandBuffer, frontFace);
        RVK_TRACE("server vkCmdSetFrontFaceEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineRasterizationModeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkLineRasterizationModeEXT lineRasterizationMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineRasterizationMode)) return false;
        RVK_TRACE("server vkCmdSetLineRasterizationModeEXT(commandBuffer={:#x}, lineRasterizationMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(lineRasterizationMode));
        s.on_vkCmdSetLineRasterizationModeEXT(commandBuffer, lineRasterizationMode);
        RVK_TRACE("server vkCmdSetLineRasterizationModeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineStippleEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t lineStippleFactor{};
        uint16_t lineStipplePattern{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(lineStippleFactor)) return false;
        if (!r.read_pod(lineStipplePattern)) return false;
        RVK_TRACE("server vkCmdSetLineStippleEXT(commandBuffer={:#x}, lineStippleFactor={}, lineStipplePattern={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(lineStippleFactor), static_cast<long long>(lineStipplePattern));
        s.on_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern);
        RVK_TRACE("server vkCmdSetLineStippleEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLineStippleEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stippledLineEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stippledLineEnable)) return false;
        RVK_TRACE("server vkCmdSetLineStippleEnableEXT(commandBuffer={:#x}, stippledLineEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stippledLineEnable));
        s.on_vkCmdSetLineStippleEnableEXT(commandBuffer, stippledLineEnable);
        RVK_TRACE("server vkCmdSetLineStippleEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetLogicOpEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 logicOpEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(logicOpEnable)) return false;
        RVK_TRACE("server vkCmdSetLogicOpEnableEXT(commandBuffer={:#x}, logicOpEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(logicOpEnable));
        s.on_vkCmdSetLogicOpEnableEXT(commandBuffer, logicOpEnable);
        RVK_TRACE("server vkCmdSetLogicOpEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPolygonModeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPolygonMode polygonMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(polygonMode)) return false;
        RVK_TRACE("server vkCmdSetPolygonModeEXT(commandBuffer={:#x}, polygonMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(polygonMode));
        s.on_vkCmdSetPolygonModeEXT(commandBuffer, polygonMode);
        RVK_TRACE("server vkCmdSetPolygonModeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetPrimitiveTopologyEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPrimitiveTopology primitiveTopology{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(primitiveTopology)) return false;
        RVK_TRACE("server vkCmdSetPrimitiveTopologyEXT(commandBuffer={:#x}, primitiveTopology={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(primitiveTopology));
        s.on_vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
        RVK_TRACE("server vkCmdSetPrimitiveTopologyEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetProvokingVertexModeEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkProvokingVertexModeEXT provokingVertexMode{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(provokingVertexMode)) return false;
        RVK_TRACE("server vkCmdSetProvokingVertexModeEXT(commandBuffer={:#x}, provokingVertexMode={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(provokingVertexMode));
        s.on_vkCmdSetProvokingVertexModeEXT(commandBuffer, provokingVertexMode);
        RVK_TRACE("server vkCmdSetProvokingVertexModeEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizationSamplesEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSampleCountFlagBits rasterizationSamples{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizationSamples)) return false;
        RVK_TRACE("server vkCmdSetRasterizationSamplesEXT(commandBuffer={:#x}, rasterizationSamples={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizationSamples));
        s.on_vkCmdSetRasterizationSamplesEXT(commandBuffer, rasterizationSamples);
        RVK_TRACE("server vkCmdSetRasterizationSamplesEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRasterizationStreamEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t rasterizationStream{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(rasterizationStream)) return false;
        RVK_TRACE("server vkCmdSetRasterizationStreamEXT(commandBuffer={:#x}, rasterizationStream={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(rasterizationStream));
        s.on_vkCmdSetRasterizationStreamEXT(commandBuffer, rasterizationStream);
        RVK_TRACE("server vkCmdSetRasterizationStreamEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRayTracingPipelineStackSizeKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t pipelineStackSize{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(pipelineStackSize)) return false;
        RVK_TRACE("server vkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer={:#x}, pipelineStackSize={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(pipelineStackSize));
        s.on_vkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer, pipelineStackSize);
        RVK_TRACE("server vkCmdSetRayTracingPipelineStackSizeKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRepresentativeFragmentTestEnableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 representativeFragmentTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(representativeFragmentTestEnable)) return false;
        RVK_TRACE("server vkCmdSetRepresentativeFragmentTestEnableNV(commandBuffer={:#x}, representativeFragmentTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(representativeFragmentTestEnable));
        s.on_vkCmdSetRepresentativeFragmentTestEnableNV(commandBuffer, representativeFragmentTestEnable);
        RVK_TRACE("server vkCmdSetRepresentativeFragmentTestEnableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetSampleLocationsEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 sampleLocationsEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(sampleLocationsEnable)) return false;
        RVK_TRACE("server vkCmdSetSampleLocationsEnableEXT(commandBuffer={:#x}, sampleLocationsEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(sampleLocationsEnable));
        s.on_vkCmdSetSampleLocationsEnableEXT(commandBuffer, sampleLocationsEnable);
        RVK_TRACE("server vkCmdSetSampleLocationsEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetSampleMaskEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSampleCountFlagBits samples{};
        std::vector<VkSampleMask> pSampleMask_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(samples)) return false;
        pSampleMask_storage.resize(static_cast<std::size_t>((samples + 31) / 32));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>((samples + 31) / 32); ++__i) {
            if (!r.read_pod(pSampleMask_storage[__i])) return false;
        }
        RVK_TRACE("server vkCmdSetSampleMaskEXT(commandBuffer={:#x}, samples={}, pSampleMask=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(samples));
        s.on_vkCmdSetSampleMaskEXT(commandBuffer, samples, pSampleMask_storage.data());
        RVK_TRACE("server vkCmdSetSampleMaskEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetScissorWithCountEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t scissorCount{};
        std::vector<VkRect2D> pScissors_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(scissorCount)) return false;
        pScissors_storage.resize(static_cast<std::size_t>(scissorCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(scissorCount); ++__i) {
            if (!deserialize_VkRect2D(r, &pScissors_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetScissorWithCountEXT(commandBuffer={:#x}, scissorCount={}, pScissors=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(scissorCount));
        s.on_vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors_storage.data());
        RVK_TRACE("server vkCmdSetScissorWithCountEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetShadingRateImageEnableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 shadingRateImageEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(shadingRateImageEnable)) return false;
        RVK_TRACE("server vkCmdSetShadingRateImageEnableNV(commandBuffer={:#x}, shadingRateImageEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(shadingRateImageEnable));
        s.on_vkCmdSetShadingRateImageEnableNV(commandBuffer, shadingRateImageEnable);
        RVK_TRACE("server vkCmdSetShadingRateImageEnableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilOpEXT: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdSetStencilOpEXT(commandBuffer={:#x}, faceMask={}, failOp={}, passOp={}, depthFailOp={}, compareOp={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(faceMask), static_cast<long long>(failOp), static_cast<long long>(passOp), static_cast<long long>(depthFailOp), static_cast<long long>(compareOp));
        s.on_vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
        RVK_TRACE("server vkCmdSetStencilOpEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetStencilTestEnableEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 stencilTestEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(stencilTestEnable)) return false;
        RVK_TRACE("server vkCmdSetStencilTestEnableEXT(commandBuffer={:#x}, stencilTestEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(stencilTestEnable));
        s.on_vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
        RVK_TRACE("server vkCmdSetStencilTestEnableEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetTessellationDomainOriginEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkTessellationDomainOrigin domainOrigin{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(domainOrigin)) return false;
        RVK_TRACE("server vkCmdSetTessellationDomainOriginEXT(commandBuffer={:#x}, domainOrigin={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(domainOrigin));
        s.on_vkCmdSetTessellationDomainOriginEXT(commandBuffer, domainOrigin);
        RVK_TRACE("server vkCmdSetTessellationDomainOriginEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportSwizzleNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstViewport{};
        uint32_t viewportCount{};
        std::vector<VkViewportSwizzleNV> pViewportSwizzles_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstViewport)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewportSwizzles_storage.resize(static_cast<std::size_t>(viewportCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
            if (!deserialize_VkViewportSwizzleNV(r, &pViewportSwizzles_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetViewportSwizzleNV(commandBuffer={:#x}, firstViewport={}, viewportCount={}, pViewportSwizzles=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstViewport), static_cast<long long>(viewportCount));
        s.on_vkCmdSetViewportSwizzleNV(commandBuffer, firstViewport, viewportCount, pViewportSwizzles_storage.data());
        RVK_TRACE("server vkCmdSetViewportSwizzleNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWScalingEnableNV: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBool32 viewportWScalingEnable{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportWScalingEnable)) return false;
        RVK_TRACE("server vkCmdSetViewportWScalingEnableNV(commandBuffer={:#x}, viewportWScalingEnable={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportWScalingEnable));
        s.on_vkCmdSetViewportWScalingEnableNV(commandBuffer, viewportWScalingEnable);
        RVK_TRACE("server vkCmdSetViewportWScalingEnableNV -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetViewportWithCountEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t viewportCount{};
        std::vector<VkViewport> pViewports_storage;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(viewportCount)) return false;
        pViewports_storage.resize(static_cast<std::size_t>(viewportCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(viewportCount); ++__i) {
            if (!deserialize_VkViewport(r, &pViewports_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkCmdSetViewportWithCountEXT(commandBuffer={:#x}, viewportCount={}, pViewports=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(viewportCount));
        s.on_vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports_storage.data());
        RVK_TRACE("server vkCmdSetViewportWithCountEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdTraceRaysIndirectKHR: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdTraceRaysIndirectKHR(commandBuffer={:#x}, pRaygenShaderBindingTable=ptr, pMissShaderBindingTable=ptr, pHitShaderBindingTable=ptr, pCallableShaderBindingTable=ptr, indirectDeviceAddress={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(indirectDeviceAddress));
        s.on_vkCmdTraceRaysIndirectKHR(commandBuffer, &pRaygenShaderBindingTable_local, &pMissShaderBindingTable_local, &pHitShaderBindingTable_local, &pCallableShaderBindingTable_local, indirectDeviceAddress);
        RVK_TRACE("server vkCmdTraceRaysIndirectKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdTraceRaysKHR: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdTraceRaysKHR(commandBuffer={:#x}, pRaygenShaderBindingTable=ptr, pMissShaderBindingTable=ptr, pHitShaderBindingTable=ptr, pCallableShaderBindingTable=ptr, width={}, height={}, depth={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(width), static_cast<long long>(height), static_cast<long long>(depth));
        s.on_vkCmdTraceRaysKHR(commandBuffer, &pRaygenShaderBindingTable_local, &pMissShaderBindingTable_local, &pHitShaderBindingTable_local, &pCallableShaderBindingTable_local, width, height, depth);
        RVK_TRACE("server vkCmdTraceRaysKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdWriteAccelerationStructuresPropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t accelerationStructureCount{};
        std::vector<VkAccelerationStructureKHR> pAccelerationStructures_storage;
        VkQueryType queryType{};
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t firstQuery{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(accelerationStructureCount)) return false;
        pAccelerationStructures_storage.resize(static_cast<std::size_t>(accelerationStructureCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(accelerationStructureCount); ++__i) {
            if (!deserialize_VkAccelerationStructureKHR(r, pAccelerationStructures_storage[__i])) return false;
        }
        if (!r.read_pod(queryType)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(firstQuery)) return false;
        RVK_TRACE("server vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer={:#x}, accelerationStructureCount={}, pAccelerationStructures=ptr, queryType={}, queryPool={:#x}, firstQuery={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(accelerationStructureCount), static_cast<long long>(queryType), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(firstQuery));
        s.on_vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, accelerationStructureCount, pAccelerationStructures_storage.data(), queryType, queryPool, firstQuery);
        RVK_TRACE("server vkCmdWriteAccelerationStructuresPropertiesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyAccelerationStructureKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
        VkResult result = s.on_vkCopyAccelerationStructureKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyAccelerationStructureKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyAccelerationStructureToMemoryKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyAccelerationStructureToMemoryInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyAccelerationStructureToMemoryInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyAccelerationStructureToMemoryKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
        VkResult result = s.on_vkCopyAccelerationStructureToMemoryKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyAccelerationStructureToMemoryKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyImageToImageEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCopyImageToImageInfo pCopyImageToImageInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyImageToImageInfo(r, &pCopyImageToImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyImageToImageEXT(device={:#x}, pCopyImageToImageInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCopyImageToImageEXT(device, &pCopyImageToImageInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyImageToImageEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyImageToMemoryEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCopyImageToMemoryInfo pCopyImageToMemoryInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyImageToMemoryInfo(r, &pCopyImageToMemoryInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyImageToMemoryEXT(device={:#x}, pCopyImageToMemoryInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCopyImageToMemoryEXT(device, &pCopyImageToMemoryInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyImageToMemoryEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyMemoryToAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkCopyMemoryToAccelerationStructureInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkCopyMemoryToAccelerationStructureInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyMemoryToAccelerationStructureKHR(device={:#x}, deferredOperation={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation));
        VkResult result = s.on_vkCopyMemoryToAccelerationStructureKHR(device, deferredOperation, &pInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyMemoryToAccelerationStructureKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCopyMemoryToImageEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkCopyMemoryToImageInfo pCopyMemoryToImageInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkCopyMemoryToImageInfo(r, &pCopyMemoryToImageInfo_local, arena)) return false;
        RVK_TRACE("server vkCopyMemoryToImageEXT(device={:#x}, pCopyMemoryToImageInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCopyMemoryToImageEXT(device, &pCopyMemoryToImageInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkCopyMemoryToImageEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR pCreateInfo_local{};
        VkAccelerationStructureKHR pAccelerationStructure_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureCreateInfoKHR(r, &pCreateInfo_local, arena)) return false;
        RVK_TRACE("server vkCreateAccelerationStructureKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pAccelerationStructure=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateAccelerationStructureKHR(device, &pCreateInfo_local, nullptr, &pAccelerationStructure_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkAccelerationStructureKHR(reply, pAccelerationStructure_out);
        RVK_TRACE("server vkCreateAccelerationStructureKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateDeferredOperationKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR pDeferredOperation_out = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        RVK_TRACE("server vkCreateDeferredOperationKHR(device={:#x}, pAllocator=ptr, pDeferredOperation=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreateDeferredOperationKHR(device, nullptr, &pDeferredOperation_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkDeferredOperationKHR(reply, pDeferredOperation_out);
        RVK_TRACE("server vkCreateDeferredOperationKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateRayTracingPipelinesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        std::vector<VkRayTracingPipelineCreateInfoKHR> pCreateInfos_storage;
        std::vector<VkPipeline> pPipelines_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, deferredOperation)) return false;
        if (!deserialize_VkPipelineCache(r, pipelineCache)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage.resize(static_cast<std::size_t>(createInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
            if (!deserialize_VkRayTracingPipelineCreateInfoKHR(r, &pCreateInfos_storage[__i], arena)) return false;
        }
        pPipelines_storage.resize(static_cast<std::size_t>(createInfoCount));
        RVK_TRACE("server vkCreateRayTracingPipelinesKHR(device={:#x}, deferredOperation={:#x}, pipelineCache={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pPipelines=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(deferredOperation), ::rvk::instr::trace_handle(pipelineCache), static_cast<long long>(createInfoCount));
        VkResult result = s.on_vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos_storage.data(), nullptr, pPipelines_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pPipelines_storage.size(); ++__k) {
            serialize_VkPipeline(reply, pPipelines_storage[__k]);
        }
        RVK_TRACE("server vkCreateRayTracingPipelinesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCreateShadersEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t createInfoCount{};
        std::vector<VkShaderCreateInfoEXT> pCreateInfos_storage;
        std::vector<VkShaderEXT> pShaders_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(createInfoCount)) return false;
        pCreateInfos_storage.resize(static_cast<std::size_t>(createInfoCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(createInfoCount); ++__i) {
            if (!deserialize_VkShaderCreateInfoEXT(r, &pCreateInfos_storage[__i], arena)) return false;
        }
        pShaders_storage.resize(static_cast<std::size_t>(createInfoCount));
        RVK_TRACE("server vkCreateShadersEXT(device={:#x}, createInfoCount={}, pCreateInfos=ptr, pAllocator=ptr, pShaders=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(createInfoCount));
        VkResult result = s.on_vkCreateShadersEXT(device, createInfoCount, pCreateInfos_storage.data(), nullptr, pShaders_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pShaders_storage.size(); ++__k) {
            serialize_VkShaderEXT(reply, pShaders_storage[__k]);
        }
        RVK_TRACE("server vkCreateShadersEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDeferredOperationJoinKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        RVK_TRACE("server vkDeferredOperationJoinKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
        VkResult result = s.on_vkDeferredOperationJoinKHR(device, operation);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkDeferredOperationJoinKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyAccelerationStructureKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureKHR(r, accelerationStructure)) return false;
        RVK_TRACE("server vkDestroyAccelerationStructureKHR(device={:#x}, accelerationStructure={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(accelerationStructure));
        s.on_vkDestroyAccelerationStructureKHR(device, accelerationStructure, nullptr);
        RVK_TRACE("server vkDestroyAccelerationStructureKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyDeferredOperationKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        RVK_TRACE("server vkDestroyDeferredOperationKHR(device={:#x}, operation={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
        s.on_vkDestroyDeferredOperationKHR(device, operation, nullptr);
        RVK_TRACE("server vkDestroyDeferredOperationKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyShaderEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkShaderEXT shader = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkShaderEXT(r, shader)) return false;
        RVK_TRACE("server vkDestroyShaderEXT(device={:#x}, shader={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(shader));
        s.on_vkDestroyShaderEXT(device, shader, nullptr);
        RVK_TRACE("server vkDestroyShaderEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetAccelerationStructureBuildSizesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureBuildTypeKHR buildType{};
        VkAccelerationStructureBuildGeometryInfoKHR pBuildInfo_local{};
        std::vector<uint32_t> pMaxPrimitiveCounts_storage;
        std::uint8_t pMaxPrimitiveCounts_present = 0;
        VkAccelerationStructureBuildSizesInfoKHR pSizeInfo_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(buildType)) return false;
        if (!deserialize_VkAccelerationStructureBuildGeometryInfoKHR(r, &pBuildInfo_local, arena)) return false;
        if (!r.read_pod(pMaxPrimitiveCounts_present)) return false;
        if (pMaxPrimitiveCounts_present) {
            pMaxPrimitiveCounts_storage.resize(static_cast<std::size_t>(pBuildInfo_local.geometryCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(pBuildInfo_local.geometryCount); ++__i) {
                if (!r.read_pod(pMaxPrimitiveCounts_storage[__i])) return false;
            }
        }
        if (!r.read_pod(pSizeInfo_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pSizeInfo_out), arena)) return false;
        RVK_TRACE("server vkGetAccelerationStructureBuildSizesKHR(device={:#x}, buildType={}, pBuildInfo=ptr, pMaxPrimitiveCounts=ptr, pSizeInfo=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(buildType));
        s.on_vkGetAccelerationStructureBuildSizesKHR(device, buildType, &pBuildInfo_local, pMaxPrimitiveCounts_present ? pMaxPrimitiveCounts_storage.data() : nullptr, &pSizeInfo_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkAccelerationStructureBuildSizesInfoKHR(reply, pSizeInfo_out);
        RVK_TRACE("server vkGetAccelerationStructureBuildSizesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetAccelerationStructureDeviceAddressKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureDeviceAddressInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureDeviceAddressInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkGetAccelerationStructureDeviceAddressKHR(device={:#x}, pInfo=ptr)",::rvk::instr::trace_handle(device));
        VkDeviceAddress result = s.on_vkGetAccelerationStructureDeviceAddressKHR(device, &pInfo_local);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetAccelerationStructureDeviceAddressKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeferredOperationMaxConcurrencyKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        RVK_TRACE("server vkGetDeferredOperationMaxConcurrencyKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
        uint32_t result = s.on_vkGetDeferredOperationMaxConcurrencyKHR(device, operation);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkGetDeferredOperationMaxConcurrencyKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeferredOperationResultKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeferredOperationKHR operation = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeferredOperationKHR(r, operation)) return false;
        RVK_TRACE("server vkGetDeferredOperationResultKHR(device={:#x}, operation={:#x})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(operation));
        VkResult result = s.on_vkGetDeferredOperationResultKHR(device, operation);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkGetDeferredOperationResultKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceAccelerationStructureCompatibilityKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkAccelerationStructureVersionInfoKHR pVersionInfo_local{};
        VkAccelerationStructureCompatibilityKHR pCompatibility_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkAccelerationStructureVersionInfoKHR(r, &pVersionInfo_local, arena)) return false;
        RVK_TRACE("server vkGetDeviceAccelerationStructureCompatibilityKHR(device={:#x}, pVersionInfo=ptr, pCompatibility=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceAccelerationStructureCompatibilityKHR(device, &pVersionInfo_local, &pCompatibility_out);
        reply.write_pod<std::int32_t>(0);
        reply.write_pod(pCompatibility_out);
        RVK_TRACE("server vkGetDeviceAccelerationStructureCompatibilityKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout2EXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource2 pSubresource_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!deserialize_VkImageSubresource2(r, &pSubresource_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        RVK_TRACE("server vkGetImageSubresourceLayout2EXT(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkGetImageSubresourceLayout2EXT(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        RVK_TRACE("server vkGetImageSubresourceLayout2EXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceFragmentShadingRatesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pFragmentShadingRateCount_local = 0;
        std::vector<VkPhysicalDeviceFragmentShadingRateKHR> pFragmentShadingRates_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pFragmentShadingRateCount_local)) return false;
            pFragmentShadingRates_storage.resize(pFragmentShadingRateCount_local);
            for (auto& __e : pFragmentShadingRates_storage) { __e.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR; __e.pNext = nullptr; }
        }
        RVK_TRACE("server vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice={:#x}, pFragmentShadingRateCount=ptr, pFragmentShadingRates=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &pFragmentShadingRateCount_local, array_present ? pFragmentShadingRates_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pFragmentShadingRateCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pFragmentShadingRateCount_local; ++k) {
                serialize_VkPhysicalDeviceFragmentShadingRateKHR(reply, pFragmentShadingRates_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceFragmentShadingRatesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t firstGroup{};
        uint32_t groupCount{};
        size_t dataSize{};
        std::vector<std::uint8_t> pData_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (!r.read_pod(firstGroup)) return false;
        if (!r.read_pod(groupCount)) return false;
        {
            std::uint64_t __wide = 0;
            if (!r.read_pod(__wide)) return false;
            dataSize = static_cast<size_t>(__wide);
        }
        pData_storage.resize(static_cast<std::size_t>(dataSize));
        RVK_TRACE("server vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device={:#x}, pipeline={:#x}, firstGroup={}, groupCount={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(firstGroup), static_cast<long long>(groupCount), static_cast<long long>(dataSize));
        VkResult result = s.on_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        if (!pData_storage.empty()) {
            reply.write(pData_storage.data(), pData_storage.size());
        }
        RVK_TRACE("server vkGetRayTracingCaptureReplayShaderGroupHandlesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingShaderGroupHandlesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t firstGroup{};
        uint32_t groupCount{};
        size_t dataSize{};
        std::vector<std::uint8_t> pData_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (!r.read_pod(firstGroup)) return false;
        if (!r.read_pod(groupCount)) return false;
        {
            std::uint64_t __wide = 0;
            if (!r.read_pod(__wide)) return false;
            dataSize = static_cast<size_t>(__wide);
        }
        pData_storage.resize(static_cast<std::size_t>(dataSize));
        RVK_TRACE("server vkGetRayTracingShaderGroupHandlesKHR(device={:#x}, pipeline={:#x}, firstGroup={}, groupCount={}, dataSize={}, pData=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(firstGroup), static_cast<long long>(groupCount), static_cast<long long>(dataSize));
        VkResult result = s.on_vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        if (!pData_storage.empty()) {
            reply.write(pData_storage.data(), pData_storage.size());
        }
        RVK_TRACE("server vkGetRayTracingShaderGroupHandlesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRayTracingShaderGroupStackSizeKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t group{};
        VkShaderGroupShaderKHR groupShader{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipeline(r, pipeline)) return false;
        if (!r.read_pod(group)) return false;
        if (!r.read_pod(groupShader)) return false;
        RVK_TRACE("server vkGetRayTracingShaderGroupStackSizeKHR(device={:#x}, pipeline={:#x}, group={}, groupShader={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipeline), static_cast<long long>(group), static_cast<long long>(groupShader));
        VkDeviceSize result = s.on_vkGetRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader);
        reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));
        RVK_TRACE("server vkGetRayTracingShaderGroupStackSizeKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkTransitionImageLayoutEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t transitionCount{};
        std::vector<VkHostImageLayoutTransitionInfo> pTransitions_storage;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(transitionCount)) return false;
        pTransitions_storage.resize(static_cast<std::size_t>(transitionCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(transitionCount); ++__i) {
            if (!deserialize_VkHostImageLayoutTransitionInfo(r, &pTransitions_storage[__i], arena)) return false;
        }
        RVK_TRACE("server vkTransitionImageLayoutEXT(device={:#x}, transitionCount={}, pTransitions=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(transitionCount));
        VkResult result = s.on_vkTransitionImageLayoutEXT(device, transitionCount, pTransitions_storage.data());
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkTransitionImageLayoutEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkWriteAccelerationStructuresPropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t accelerationStructureCount{};
        std::vector<VkAccelerationStructureKHR> pAccelerationStructures_storage;
        VkQueryType queryType{};
        size_t dataSize{};
        std::vector<std::uint8_t> pData_storage;
        size_t stride{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(accelerationStructureCount)) return false;
        pAccelerationStructures_storage.resize(static_cast<std::size_t>(accelerationStructureCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(accelerationStructureCount); ++__i) {
            if (!deserialize_VkAccelerationStructureKHR(r, pAccelerationStructures_storage[__i])) return false;
        }
        if (!r.read_pod(queryType)) return false;
        {
            std::uint64_t __wide = 0;
            if (!r.read_pod(__wide)) return false;
            dataSize = static_cast<size_t>(__wide);
        }
        pData_storage.resize(static_cast<std::size_t>(dataSize));
        {
            std::uint64_t __wide = 0;
            if (!r.read_pod(__wide)) return false;
            stride = static_cast<size_t>(__wide);
        }
        RVK_TRACE("server vkWriteAccelerationStructuresPropertiesKHR(device={:#x}, accelerationStructureCount={}, pAccelerationStructures=ptr, queryType={}, dataSize={}, pData=ptr, stride={})",::rvk::instr::trace_handle(device), static_cast<long long>(accelerationStructureCount), static_cast<long long>(queryType), static_cast<long long>(dataSize), static_cast<long long>(stride));
        VkResult result = s.on_vkWriteAccelerationStructuresPropertiesKHR(device, accelerationStructureCount, pAccelerationStructures_storage.data(), queryType, dataSize, pData_storage.data(), stride);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        if (!pData_storage.empty()) {
            reply.write(pData_storage.data(), pData_storage.size());
        }
        RVK_TRACE("server vkWriteAccelerationStructuresPropertiesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetCalibratedTimestampsEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t timestampCount{};
        std::vector<VkCalibratedTimestampInfoKHR> pTimestampInfos_storage;
        std::vector<uint64_t> pTimestamps_storage;
        uint64_t pMaxDeviation_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(timestampCount)) return false;
        pTimestampInfos_storage.resize(static_cast<std::size_t>(timestampCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(timestampCount); ++__i) {
            if (!deserialize_VkCalibratedTimestampInfoKHR(r, &pTimestampInfos_storage[__i], arena)) return false;
        }
        pTimestamps_storage.resize(static_cast<std::size_t>(timestampCount));
        RVK_TRACE("server vkGetCalibratedTimestampsEXT(device={:#x}, timestampCount={}, pTimestampInfos=ptr, pTimestamps=ptr, pMaxDeviation=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(timestampCount));
        VkResult result = s.on_vkGetCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos_storage.data(), pTimestamps_storage.data(), &pMaxDeviation_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        for (std::uint32_t __k = 0; __k < pTimestamps_storage.size(); ++__k) {
            reply.write_pod(pTimestamps_storage[__k]);
        }
        reply.write_pod(pMaxDeviation_out);
        RVK_TRACE("server vkGetCalibratedTimestampsEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::uint8_t array_present = 0;
        std::uint32_t pTimeDomainCount_local = 0;
        std::vector<VkTimeDomainKHR> pTimeDomains_storage;
        if (!deserialize_VkPhysicalDevice(r, physicalDevice)) return false;
        if (!r.read_pod(array_present)) return false;
        if (array_present) {
            if (!r.read_pod(pTimeDomainCount_local)) return false;
            pTimeDomains_storage.resize(pTimeDomainCount_local);
        }
        RVK_TRACE("server vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice={:#x}, pTimeDomainCount=ptr, pTimeDomains=ptr)",::rvk::instr::trace_handle(physicalDevice));
        VkResult result = s.on_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice, &pTimeDomainCount_local, array_present ? pTimeDomains_storage.data() : nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pTimeDomainCount_local);
        if (array_present) {
            for (std::uint32_t k = 0; k < pTimeDomainCount_local; ++k) {
                reply.write_pod(pTimeDomains_storage[k]);
            }
        }
        RVK_TRACE("server vkGetPhysicalDeviceCalibrateableTimeDomainsEXT -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRenderingAttachmentLocationsKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingAttachmentLocationInfo pLocationInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingAttachmentLocationInfo(r, &pLocationInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdSetRenderingAttachmentLocationsKHR(commandBuffer={:#x}, pLocationInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdSetRenderingAttachmentLocationsKHR(commandBuffer, &pLocationInfo_local);
        RVK_TRACE("server vkCmdSetRenderingAttachmentLocationsKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdSetRenderingInputAttachmentIndicesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkRenderingInputAttachmentIndexInfo pInputAttachmentIndexInfo_local{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkRenderingInputAttachmentIndexInfo(r, &pInputAttachmentIndexInfo_local, arena)) return false;
        RVK_TRACE("server vkCmdSetRenderingInputAttachmentIndicesKHR(commandBuffer={:#x}, pInputAttachmentIndexInfo=ptr)",::rvk::instr::trace_handle(commandBuffer));
        s.on_vkCmdSetRenderingInputAttachmentIndicesKHR(commandBuffer, &pInputAttachmentIndexInfo_local);
        RVK_TRACE("server vkCmdSetRenderingInputAttachmentIndicesKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindIndexBuffer2KHR: {
        ::av::WireArena arena;
        (void)arena;
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
        RVK_TRACE("server vkCmdBindIndexBuffer2KHR(commandBuffer={:#x}, buffer={:#x}, offset={}, size={}, indexType={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(buffer), static_cast<long long>(offset), static_cast<long long>(size), static_cast<long long>(indexType));
        s.on_vkCmdBindIndexBuffer2KHR(commandBuffer, buffer, offset, size, indexType);
        RVK_TRACE("server vkCmdBindIndexBuffer2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCreatePipelineBinariesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineBinaryCreateInfoKHR pCreateInfo_local{};
        VkPipelineBinaryHandlesInfoKHR pBinaries_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineBinaryCreateInfoKHR(r, &pCreateInfo_local, arena)) return false;
        if (!r.read_pod(pBinaries_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pBinaries_out), arena)) return false;
        RVK_TRACE("server vkCreatePipelineBinariesKHR(device={:#x}, pCreateInfo=ptr, pAllocator=ptr, pBinaries=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkCreatePipelineBinariesKHR(device, &pCreateInfo_local, nullptr, &pBinaries_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkPipelineBinaryHandlesInfoKHR(reply, pBinaries_out);
        RVK_TRACE("server vkCreatePipelineBinariesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkDestroyPipelineBinaryKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineBinaryKHR pipelineBinary = VK_NULL_HANDLE;
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineBinaryKHR(r, pipelineBinary)) return false;
        RVK_TRACE("server vkDestroyPipelineBinaryKHR(device={:#x}, pipelineBinary={:#x}, pAllocator=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(pipelineBinary));
        s.on_vkDestroyPipelineBinaryKHR(device, pipelineBinary, nullptr);
        RVK_TRACE("server vkDestroyPipelineBinaryKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetDeviceImageSubresourceLayoutKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceImageSubresourceInfo pInfo_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceImageSubresourceInfo(r, &pInfo_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        RVK_TRACE("server vkGetDeviceImageSubresourceLayoutKHR(device={:#x}, pInfo=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetDeviceImageSubresourceLayoutKHR(device, &pInfo_local, &pLayout_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        RVK_TRACE("server vkGetDeviceImageSubresourceLayoutKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetImageSubresourceLayout2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresource2 pSubresource_local{};
        VkSubresourceLayout2 pLayout_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImage(r, image)) return false;
        if (!deserialize_VkImageSubresource2(r, &pSubresource_local, arena)) return false;
        if (!r.read_pod(pLayout_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pLayout_out), arena)) return false;
        RVK_TRACE("server vkGetImageSubresourceLayout2KHR(device={:#x}, image={:#x}, pSubresource=ptr, pLayout=ptr)",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(image));
        s.on_vkGetImageSubresourceLayout2KHR(device, image, &pSubresource_local, &pLayout_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkSubresourceLayout2(reply, pLayout_out);
        RVK_TRACE("server vkGetImageSubresourceLayout2KHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetPipelineKeyKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineCreateInfoKHR pPipelineCreateInfo_local{};
        VkPipelineBinaryKeyKHR pPipelineKey_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkPipelineCreateInfoKHR(r, &pPipelineCreateInfo_local, arena)) return false;
        if (!r.read_pod(pPipelineKey_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pPipelineKey_out), arena)) return false;
        RVK_TRACE("server vkGetPipelineKeyKHR(device={:#x}, pPipelineCreateInfo=ptr, pPipelineKey=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkGetPipelineKeyKHR(device, &pPipelineCreateInfo_local, &pPipelineKey_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkPipelineBinaryKeyKHR(reply, pPipelineKey_out);
        RVK_TRACE("server vkGetPipelineKeyKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetRenderingAreaGranularityKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkRenderingAreaInfo pRenderingAreaInfo_local{};
        VkExtent2D pGranularity_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkRenderingAreaInfo(r, &pRenderingAreaInfo_local, arena)) return false;
        RVK_TRACE("server vkGetRenderingAreaGranularityKHR(device={:#x}, pRenderingAreaInfo=ptr, pGranularity=ptr)",::rvk::instr::trace_handle(device));
        s.on_vkGetRenderingAreaGranularityKHR(device, &pRenderingAreaInfo_local, &pGranularity_out);
        reply.write_pod<std::int32_t>(0);
        serialize_VkExtent2D(reply, pGranularity_out);
        RVK_TRACE("server vkGetRenderingAreaGranularityKHR -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkReleaseCapturedPipelineDataKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkReleaseCapturedPipelineDataInfoKHR pInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkReleaseCapturedPipelineDataInfoKHR(r, &pInfo_local, arena)) return false;
        RVK_TRACE("server vkReleaseCapturedPipelineDataKHR(device={:#x}, pInfo=ptr, pAllocator=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkReleaseCapturedPipelineDataKHR(device, &pInfo_local, nullptr);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkReleaseCapturedPipelineDataKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginQueryIndexedEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        VkQueryControlFlags flags{};
        uint32_t index{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(flags)) return false;
        if (!r.read_pod(index)) return false;
        RVK_TRACE("server vkCmdBeginQueryIndexedEXT(commandBuffer={:#x}, queryPool={:#x}, query={}, flags={}, index={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(flags), static_cast<long long>(index));
        s.on_vkCmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index);
        RVK_TRACE("server vkCmdBeginQueryIndexedEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBeginTransformFeedbackEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstCounterBuffer{};
        uint32_t counterBufferCount{};
        std::vector<VkBuffer> pCounterBuffers_storage;
        std::vector<VkDeviceSize> pCounterBufferOffsets_storage;
        std::uint8_t pCounterBufferOffsets_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstCounterBuffer)) return false;
        if (!r.read_pod(counterBufferCount)) return false;
        pCounterBuffers_storage.resize(static_cast<std::size_t>(counterBufferCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
            if (!deserialize_VkBuffer(r, pCounterBuffers_storage[__i])) return false;
        }
        if (!r.read_pod(pCounterBufferOffsets_present)) return false;
        if (pCounterBufferOffsets_present) {
            pCounterBufferOffsets_storage.resize(static_cast<std::size_t>(counterBufferCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
                if (!r.read_pod(pCounterBufferOffsets_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdBeginTransformFeedbackEXT(commandBuffer={:#x}, firstCounterBuffer={}, counterBufferCount={}, pCounterBuffers=ptr, pCounterBufferOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstCounterBuffer), static_cast<long long>(counterBufferCount));
        s.on_vkCmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers_storage.data(), pCounterBufferOffsets_present ? pCounterBufferOffsets_storage.data() : nullptr);
        RVK_TRACE("server vkCmdBeginTransformFeedbackEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdBindTransformFeedbackBuffersEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstBinding{};
        uint32_t bindingCount{};
        std::vector<VkBuffer> pBuffers_storage;
        std::vector<VkDeviceSize> pOffsets_storage;
        std::vector<VkDeviceSize> pSizes_storage;
        std::uint8_t pSizes_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstBinding)) return false;
        if (!r.read_pod(bindingCount)) return false;
        pBuffers_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!deserialize_VkBuffer(r, pBuffers_storage[__i])) return false;
        }
        pOffsets_storage.resize(static_cast<std::size_t>(bindingCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
            if (!r.read_pod(pOffsets_storage[__i])) return false;
        }
        if (!r.read_pod(pSizes_present)) return false;
        if (pSizes_present) {
            pSizes_storage.resize(static_cast<std::size_t>(bindingCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(bindingCount); ++__i) {
                if (!r.read_pod(pSizes_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdBindTransformFeedbackBuffersEXT(commandBuffer={:#x}, firstBinding={}, bindingCount={}, pBuffers=ptr, pOffsets=ptr, pSizes=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstBinding), static_cast<long long>(bindingCount));
        s.on_vkCmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers_storage.data(), pOffsets_storage.data(), pSizes_present ? pSizes_storage.data() : nullptr);
        RVK_TRACE("server vkCmdBindTransformFeedbackBuffersEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdDrawIndirectByteCountEXT: {
        ::av::WireArena arena;
        (void)arena;
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
        if (!r.read_pod(counterBufferOffset)) return false;
        if (!r.read_pod(counterOffset)) return false;
        if (!r.read_pod(vertexStride)) return false;
        RVK_TRACE("server vkCmdDrawIndirectByteCountEXT(commandBuffer={:#x}, instanceCount={}, firstInstance={}, counterBuffer={:#x}, counterBufferOffset={}, counterOffset={}, vertexStride={})",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(instanceCount), static_cast<long long>(firstInstance), ::rvk::instr::trace_handle(counterBuffer), static_cast<long long>(counterBufferOffset), static_cast<long long>(counterOffset), static_cast<long long>(vertexStride));
        s.on_vkCmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
        RVK_TRACE("server vkCmdDrawIndirectByteCountEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndQueryIndexedEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t query{};
        uint32_t index{};
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!deserialize_VkQueryPool(r, queryPool)) return false;
        if (!r.read_pod(query)) return false;
        if (!r.read_pod(index)) return false;
        RVK_TRACE("server vkCmdEndQueryIndexedEXT(commandBuffer={:#x}, queryPool={:#x}, query={}, index={})",::rvk::instr::trace_handle(commandBuffer), ::rvk::instr::trace_handle(queryPool), static_cast<long long>(query), static_cast<long long>(index));
        s.on_vkCmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index);
        RVK_TRACE("server vkCmdEndQueryIndexedEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkCmdEndTransformFeedbackEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        uint32_t firstCounterBuffer{};
        uint32_t counterBufferCount{};
        std::vector<VkBuffer> pCounterBuffers_storage;
        std::vector<VkDeviceSize> pCounterBufferOffsets_storage;
        std::uint8_t pCounterBufferOffsets_present = 0;
        if (!deserialize_VkCommandBuffer(r, commandBuffer)) return false;
        if (!r.read_pod(firstCounterBuffer)) return false;
        if (!r.read_pod(counterBufferCount)) return false;
        pCounterBuffers_storage.resize(static_cast<std::size_t>(counterBufferCount));
        for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
            if (!deserialize_VkBuffer(r, pCounterBuffers_storage[__i])) return false;
        }
        if (!r.read_pod(pCounterBufferOffsets_present)) return false;
        if (pCounterBufferOffsets_present) {
            pCounterBufferOffsets_storage.resize(static_cast<std::size_t>(counterBufferCount));
            for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>(counterBufferCount); ++__i) {
                if (!r.read_pod(pCounterBufferOffsets_storage[__i])) return false;
            }
        }
        RVK_TRACE("server vkCmdEndTransformFeedbackEXT(commandBuffer={:#x}, firstCounterBuffer={}, counterBufferCount={}, pCounterBuffers=ptr, pCounterBufferOffsets=ptr)",::rvk::instr::trace_handle(commandBuffer), static_cast<long long>(firstCounterBuffer), static_cast<long long>(counterBufferCount));
        s.on_vkCmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers_storage.data(), pCounterBufferOffsets_present ? pCounterBufferOffsets_storage.data() : nullptr);
        RVK_TRACE("server vkCmdEndTransformFeedbackEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkSetDeviceMemoryPriorityEXT: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        float priority{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkDeviceMemory(r, memory)) return false;
        if (!r.read_pod(priority)) return false;
        RVK_TRACE("server vkSetDeviceMemoryPriorityEXT(device={:#x}, memory={:#x}, priority={})",::rvk::instr::trace_handle(device), ::rvk::instr::trace_handle(memory), priority);
        s.on_vkSetDeviceMemoryPriorityEXT(device, memory, priority);
        RVK_TRACE("server vkSetDeviceMemoryPriorityEXT -> (void)");
        return true;
    }
    case ::av::vkproto::kCmd_vkGetFenceFdKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkFenceGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkFenceGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        RVK_TRACE("server vkGetFenceFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkGetFenceFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pFd_out);
        RVK_TRACE("server vkGetFenceFdKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetMemoryFdKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkMemoryGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkMemoryGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        RVK_TRACE("server vkGetMemoryFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkGetMemoryFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pFd_out);
        RVK_TRACE("server vkGetMemoryFdKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetMemoryFdPropertiesKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkExternalMemoryHandleTypeFlagBits handleType{};
        int fd{};
        VkMemoryFdPropertiesKHR pMemoryFdProperties_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!r.read_pod(handleType)) return false;
        if (!r.read_pod(fd)) return false;
        if (!r.read_pod(pMemoryFdProperties_out.sType)) return false;
        if (!deserialize_pnext_chain(r, reinterpret_cast<VkBaseOutStructure*>(&pMemoryFdProperties_out), arena)) return false;
        RVK_TRACE("server vkGetMemoryFdPropertiesKHR(device={:#x}, handleType={}, fd={}, pMemoryFdProperties=ptr)",::rvk::instr::trace_handle(device), static_cast<long long>(handleType), static_cast<long long>(fd));
        VkResult result = s.on_vkGetMemoryFdPropertiesKHR(device, handleType, fd, &pMemoryFdProperties_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        serialize_VkMemoryFdPropertiesKHR(reply, pMemoryFdProperties_out);
        RVK_TRACE("server vkGetMemoryFdPropertiesKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkGetSemaphoreFdKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreGetFdInfoKHR pGetFdInfo_local{};
        int pFd_out{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkSemaphoreGetFdInfoKHR(r, &pGetFdInfo_local, arena)) return false;
        RVK_TRACE("server vkGetSemaphoreFdKHR(device={:#x}, pGetFdInfo=ptr, pFd=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkGetSemaphoreFdKHR(device, &pGetFdInfo_local, &pFd_out);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        reply.write_pod(pFd_out);
        RVK_TRACE("server vkGetSemaphoreFdKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkImportFenceFdKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImportFenceFdInfoKHR pImportFenceFdInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImportFenceFdInfoKHR(r, &pImportFenceFdInfo_local, arena)) return false;
        RVK_TRACE("server vkImportFenceFdKHR(device={:#x}, pImportFenceFdInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkImportFenceFdKHR(device, &pImportFenceFdInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkImportFenceFdKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkImportSemaphoreFdKHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkImportSemaphoreFdInfoKHR pImportSemaphoreFdInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkImportSemaphoreFdInfoKHR(r, &pImportSemaphoreFdInfo_local, arena)) return false;
        RVK_TRACE("server vkImportSemaphoreFdKHR(device={:#x}, pImportSemaphoreFdInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkImportSemaphoreFdKHR(device, &pImportSemaphoreFdInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkImportSemaphoreFdKHR -> {}", static_cast<long long>(result));
        return true;
    }
    case ::av::vkproto::kCmd_vkUnmapMemory2KHR: {
        ::av::WireArena arena;
        (void)arena;
        VkDevice device = VK_NULL_HANDLE;
        VkMemoryUnmapInfo pMemoryUnmapInfo_local{};
        if (!deserialize_VkDevice(r, device)) return false;
        if (!deserialize_VkMemoryUnmapInfo(r, &pMemoryUnmapInfo_local, arena)) return false;
        RVK_TRACE("server vkUnmapMemory2KHR(device={:#x}, pMemoryUnmapInfo=ptr)",::rvk::instr::trace_handle(device));
        VkResult result = s.on_vkUnmapMemory2KHR(device, &pMemoryUnmapInfo_local);
        reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));
        RVK_TRACE("server vkUnmapMemory2KHR -> {}", static_cast<long long>(result));
        return true;
    }
    default:
        return false;
    }
}

}  // namespace av::vkproto
