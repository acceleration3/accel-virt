/* Hostile swapchain creation (feature 046): the application's
 * VkSwapchainCreateInfoKHR is foreign bytes, so every field of it comes from
 * the fuzzer - the structure type, the image count, extent, format, colour
 * space, usage, transform, alpha and present mode - over a window whose client
 * size the fuzzer chooses too. A swapchain that says it was made must hold its
 * invariants and have exactly the images it promised; one that refuses must
 * leave no host object and no guest memory behind. */
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "icd/vulkan/swapchain.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

/* The structure's fields, two window dimensions and the refusal selector. */
constexpr std::size_t   needed_bytes = 44u;
/* Large enough for a swapchain of the greatest image count, small enough that
 * a whole corpus runs fast. */
constexpr std::uint32_t max_side = 8u;

std::uint32_t read_u32(const std::uint8_t *bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8u |
           static_cast<std::uint32_t>(bytes[2]) << 16u | static_cast<std::uint32_t>(bytes[3]) << 24u;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < needed_bytes) return 0;
    const std::uint32_t width = read_u32(data + 36u) % (max_side + 1u);
    const std::uint32_t height = read_u32(data + 40u) % (max_side + 1u);

    av::MockPlatform          memory;
    av::MockKmt               kmt;
    av::RecordingLog          log;
    av::ScriptedVulkanDevice  host;
    av::ScriptedWindowSurface windows;
    void *const               window = reinterpret_cast<void *>(0x10u);
    windows.add_dc(reinterpret_cast<void *>(0x20u), window, width, height);
    kmt.add_adapter(av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                    av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem),
                    av::AdapterScenario::wddm_3_2);
    std::uint32_t adapters[2] = {};
    std::uint32_t found = 0u;
    if (kmt.enumerate_adapters(adapters, 2u, found) != av::Status::ok || found == 0u) return 0;
    std::uint32_t kmt_device = 0u;
    if (kmt.create_device(adapters[0], kmt_device) != av::Status::ok) return 0;

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
    /* Every step of making the host objects, refused in turn. */
    const char *const steps[] = {nullptr, "vkCreateBuffer", "vkMapMemory", "vkCreateImage", "vkCreateFence"};
    const char *const refused = steps[data[40u] % (sizeof steps / sizeof steps[0])];
    if (refused != nullptr) host.refuse(refused, 1u + (data[41u] % 2u), VK_ERROR_OUT_OF_DEVICE_MEMORY);
    if ((data[42u] & 1u) != 0u) memory.fail_allocation_at(1u);

    av::icd::Swapchain chain(memory, windows, host, host.device(), host.physical(), kmt, log, adapters[0], kmt_device);
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
