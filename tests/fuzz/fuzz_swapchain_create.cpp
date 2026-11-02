#include "harness/icd/open_connection.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "icd/vulkan/swapchain.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {
constexpr std::size_t   needed_bytes = 44u;
constexpr std::uint32_t max_side = 8u;

std::uint32_t read_u32(const std::uint8_t *bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8u |
           static_cast<std::uint32_t>(bytes[2]) << 16u | static_cast<std::uint32_t>(bytes[3]) << 24u;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < needed_bytes) return 0;
    const std::uint32_t width = read_u32(data + 36u) % (max_side + 1u);
    const std::uint32_t height = read_u32(data + 40u) % (max_side + 1u);

    av::MockPlatform          memory;
    av::OpenConnection        open;
    av::ScriptedVulkanDevice  host;
    av::ScriptedWindowSurface windows;
    av::ScriptedWindowChains  chains;
    void *const               window = reinterpret_cast<void *>(0x10u);
    windows.add_dc(reinterpret_cast<void *>(0x20u), window, width, height);

    VkSwapchainCreateInfoKHR info = {};
    info.sType = static_cast<VkStructureType>(read_u32(data));
    info.flags = read_u32(data + 4u);
    info.minImageCount = read_u32(data + 8u) % 16u;
    info.imageFormat = static_cast<VkFormat>(read_u32(data + 12u) % 200u);
    info.imageColorSpace = static_cast<VkColorSpaceKHR>(read_u32(data + 16u) % 4u);
    info.imageExtent = {read_u32(data + 20u) % (max_side + 2u), read_u32(data + 24u) % (max_side + 2u)};
    info.imageArrayLayers = read_u32(data + 28u) % 4u;
    info.imageUsage = read_u32(data + 32u);
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(1u << (data[36u] % 9u));
    info.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(1u << (data[37u] % 4u));
    info.presentMode = static_cast<VkPresentModeKHR>(data[38u] % 6u);
    info.clipped = data[39u] & 1u;
    const char *const steps[] = {nullptr, "vkCreateBuffer", "vkMapMemory", "vkCreateImage", "vkCreateFence"};
    const char *const refused = steps[data[40u] % (sizeof steps / sizeof steps[0])];
    if (refused != nullptr) host.refuse(refused, 1u + (data[41u] % 2u), VK_ERROR_OUT_OF_DEVICE_MEMORY);
    if ((data[42u] & 1u) != 0u) memory.fail_allocation_at(1u);

    av::icd::Swapchain chain(memory, windows, chains, host, host.device(), host.physical(), open.kmt(), open.log(),
                             open.connection());
    av::icd::SurfaceProperties properties(windows, window);
    const VkResult made = chain.initialize(info, window, properties);
    if (!chain.invariants_hold()) __builtin_trap();
    std::uint32_t count = 0u;
    if (chain.images(count, nullptr) != VK_SUCCESS) __builtin_trap();
    if (made == VK_SUCCESS) {
        if (count != info.minImageCount || host.images() != info.minImageCount) __builtin_trap();
        std::uint32_t index = 0u;
        if (chain.acquire(0u, index) != VK_SUCCESS || index >= count) __builtin_trap();
        static_cast<void>(chain.present(host.queue(), index, 0u, nullptr));
        if (!chain.invariants_hold()) __builtin_trap();
    } else if (count != 0u || host.images() != 0u || host.memories() != 0u || memory.live_allocations() != 0u) {
        __builtin_trap();
    }
    return 0;
}
