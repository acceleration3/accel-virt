/* A Direct3D 11 texture a guest shares, opened by Vulkan on the same GPU
 * (feature 053, docs/shared-surfaces.md step 5): the texture is made over
 * memory the D3D backend exports, a second device imports that memory as the
 * Vulkan backend does - not dedicated, in the first type the dma-buf allows -
 * and binds the image ExportableImageInfo describes to it, a pattern is copied
 * into that image, and Direct3D reads back every texel where Vulkan put it.
 * This is how a Vulkan swapchain's frames reach a DXGI chain, in both channel
 * orders a chain's slot has. The texture is narrower than a whole number of
 * 256-byte rows, so the rows the description places past each row's texels
 * are where both devices must look. A host without DXVK and Vulkan on one GPU
 * returns without judging. */
#include "proto/d3d/resource_misc.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/vulkan/exportable_image_info.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <vulkan/vulkan_core.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libdxvk\n"
           "leak:libvulkan_radeon\n"
           "leak:libvkd3d\n"
           "leak:libdbus-1\n"
           "leak:libz\n";
}

namespace {

using namespace av::d3dproto;

/* 244 bytes a row, which the description pitches 256 apart
 * (ExportableImageInfo): Direct3D's texture and Vulkan's image agree only
 * as both are laid out from it. */
constexpr std::uint32_t width = 61u;
constexpr std::uint32_t height = 64u;
constexpr std::uint32_t texels = width * height;
constexpr std::uint32_t format_r8g8b8a8 = 28u;
constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
/* Shared and asked for as rows, as the code of ours that makes a texture
 * another API reads asks for it (SharedRowsScope). */
constexpr std::uint32_t misc_shared = av::ResourceMisc::shared_bit | av::ResourceMisc::rows_bit;
constexpr std::uint64_t key = 0x5100000000000041ull;
constexpr std::uint32_t uuid_bytes = VK_UUID_SIZE;

/* One device on the GPU `uuid` names, with what a dma-buf import needs. */
class HostVulkan {
public:
    explicit HostVulkan(const std::uint8_t *uuid)
        : instance_(VK_NULL_HANDLE), physical_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), queue_(VK_NULL_HANDLE),
          pool_(VK_NULL_HANDLE), fd_properties_(nullptr)
    {
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.apiVersion = (1u << 22u) | (3u << 12u);   /* VK_API_VERSION_1_3, whose macro casts as C does */
        VkInstanceCreateInfo instance = {};
        instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance.pApplicationInfo = &application;
        if (vkCreateInstance(&instance, nullptr, &instance_) != VK_SUCCESS) return;
        VkPhysicalDevice physicals[8] = {};
        std::uint32_t    count = 8u;
        static_cast<void>(vkEnumeratePhysicalDevices(instance_, &count, physicals));
        for (std::uint32_t index = 0u; index < count && physical_ == VK_NULL_HANDLE; index++) {
            VkPhysicalDeviceIDProperties identity = {};
            identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            VkPhysicalDeviceProperties2 properties = {};
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties.pNext = &identity;
            vkGetPhysicalDeviceProperties2(physicals[index], &properties);
            if (std::memcmp(identity.deviceUUID, uuid, uuid_bytes) == 0) physical_ = physicals[index];
        }
        if (physical_ == VK_NULL_HANDLE) return;
        const float             priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueCount = 1u;
        queue.pQueuePriorities = &priority;
        const char *const  extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
        VkDeviceCreateInfo device = {};
        device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device.queueCreateInfoCount = 1u;
        device.pQueueCreateInfos = &queue;
        device.enabledExtensionCount = 2u;
        device.ppEnabledExtensionNames = extensions;
        if (vkCreateDevice(physical_, &device, nullptr, &device_) != VK_SUCCESS) {
            device_ = VK_NULL_HANDLE;
            return;
        }
        vkGetDeviceQueue(device_, 0u, 0u, &queue_);
        VkCommandPoolCreateInfo pool = {};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        static_cast<void>(vkCreateCommandPool(device_, &pool, nullptr, &pool_));
        fd_properties_ = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(device_, "vkGetMemoryFdPropertiesKHR"));
    }

    ~HostVulkan()
    {
        if (pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, pool_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    HostVulkan(const HostVulkan &) = delete;
    HostVulkan &operator=(const HostVulkan &) = delete;

    bool     usable() const { return device_ != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE && fd_properties_ != nullptr; }
    VkDevice device() const { return device_; }

    /* The types `fd` may be imported as, asked of the driver. */
    std::uint32_t dma_buf_types(int fd) const
    {
        VkMemoryFdPropertiesKHR answered = {};
        answered.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        if (fd_properties_(device_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &answered) != VK_SUCCESS)
            return 0u;
        return answered.memoryTypeBits;
    }

    /* The lowest type in `mask` with every property in `wanted`; 32 for none. */
    std::uint32_t type_in(std::uint32_t mask, VkMemoryPropertyFlags wanted) const
    {
        VkPhysicalDeviceMemoryProperties memory = {};
        vkGetPhysicalDeviceMemoryProperties(physical_, &memory);
        for (std::uint32_t type = 0u; type < memory.memoryTypeCount; type++)
            if ((mask & (1u << type)) != 0u && (memory.memoryTypes[type].propertyFlags & wanted) == wanted)
                return type;
        return 32u;
    }

    /* `bytes` copied into `image`, which is left GENERAL, and waited for. */
    bool upload(VkImage image, const std::uint8_t *bytes, VkDeviceSize length)
    {
        VkBufferCreateInfo created = {};
        created.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        created.size = length;
        created.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkBuffer staging = VK_NULL_HANDLE;
        if (vkCreateBuffer(device_, &created, nullptr, &staging) != VK_SUCCESS) return false;
        VkMemoryRequirements needs = {};
        vkGetBufferMemoryRequirements(device_, staging, &needs);
        VkMemoryAllocateInfo allocate = {};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = needs.size;
        allocate.memoryTypeIndex =
            type_in(needs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void          *mapped = nullptr;
        bool           ran = vkAllocateMemory(device_, &allocate, nullptr, &memory) == VK_SUCCESS &&
                   vkBindBufferMemory(device_, staging, memory, 0u) == VK_SUCCESS &&
                   vkMapMemory(device_, memory, 0u, VK_WHOLE_SIZE, 0u, &mapped) == VK_SUCCESS;
        if (ran) {
            std::memcpy(mapped, bytes, length);
            ran = copy(staging, image);
        }
        vkDestroyBuffer(device_, staging, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device_, memory, nullptr);
        return ran;
    }

private:
    bool copy(VkBuffer staging, VkImage image)
    {
        VkCommandBufferAllocateInfo allocate = {};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool_;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1u;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device_, &allocate, &commands) != VK_SUCCESS) return false;
        VkCommandBufferBeginInfo begin = {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        static_cast<void>(vkBeginCommandBuffer(commands, &begin));
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                             nullptr, 0u, nullptr, 1u, &barrier);
        VkBufferImageCopy region = {};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        region.imageExtent = {width, height, 1u};
        vkCmdCopyBufferToImage(commands, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0u;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u,
                             nullptr, 0u, nullptr, 1u, &barrier);
        static_cast<void>(vkEndCommandBuffer(commands));
        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1u;
        submit.pCommandBuffers = &commands;
        const bool ran = vkQueueSubmit(queue_, 1u, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                         vkQueueWaitIdle(queue_) == VK_SUCCESS;
        vkFreeCommandBuffers(device_, pool_, 1u, &commands);
        return ran;
    }

    VkInstance                     instance_;
    VkPhysicalDevice               physical_;
    VkDevice                       device_;
    VkQueue                        queue_;
    VkCommandPool                  pool_;
    PFN_vkGetMemoryFdPropertiesKHR fd_properties_;
};

}  // namespace

TEST(d3d_vulkan_interop_device, a_shared_texture_written_through_vulkan_reads_back_through_direct3d)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis apis;
    apis.open();
    std::uint32_t gpu = apis.gpu_count();
    for (std::uint32_t index = 0u; index < apis.gpu_count() && gpu == apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr) gpu = index;
    if (gpu == apis.gpu_count()) {
        std::fprintf(stderr, "no GPU here runs DXVK: nothing judged\n");
        return;
    }
    std::uint8_t uuid[uuid_bytes] = {};
    apis.gpu_uuid(gpu, uuid);
    HostVulkan vulkan(uuid);
    if (!vulkan.usable()) {
        std::fprintf(stderr, "no Vulkan device with dma-buf import on DXVK's GPU: nothing judged\n");
        return;
    }
    av::D3dBackend direct3d;
    av::SessionShares unshared;
    CHECK(direct3d.initialize(unshared, apis.d3d11_device(gpu)));
    av::D3dLoopback wire(direct3d);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));

    /* Every texel its own value, so a layout the two devices disagree on moves
     * texels where a clear would not show it. */
    static std::uint8_t pattern[texels * 4u];
    for (std::uint32_t at = 0u; at < texels; at++) {
        pattern[at * 4u] = static_cast<std::uint8_t>(at % width);
        pattern[at * 4u + 1u] = static_cast<std::uint8_t>(at / width);
        pattern[at * 4u + 2u] = static_cast<std::uint8_t>(at * 7u);
        pattern[at * 4u + 3u] = static_cast<std::uint8_t>(at * 3u + 1u);
    }
    const std::uint32_t formats[] = {format_r8g8b8a8, format_b8g8r8a8};
    for (std::uint32_t round = 0u; round < 2u; round++) {
        const std::uint32_t resource = round + 1u;
        ResourceDesc        shared = {};
        shared.dimension = 3u;
        shared.width = width;
        shared.height = height;
        shared.depth_or_array = 1u;
        shared.mip_levels = 1u;
        shared.format = formats[round];
        shared.sample_count = 1u;
        shared.bind_flags = bind_shader_resource | bind_render_target;
        shared.misc_flags = misc_shared;
        CHECK(av_d3d_CreateResource(wire, resource, shared, key + round));
        std::uint32_t handle_kind = 0u;
        int           fd = -1;
        CHECK(direct3d.export_object(AV_SHARED_MEMORY, resource, handle_kind, fd));
        CHECK_EQ(handle_kind, std::uint32_t{AV_SHARED_HANDLE_DMA_BUF});
        CHECK(fd >= 0);
        const off_t bytes = lseek(fd, 0, SEEK_END);
        CHECK(bytes > 0);

        const av::ExportableImageInfo described(width, height, 1u, 1u, 1u, formats[round],
                                                bind_shader_resource | bind_render_target);
        CHECK(described.supported());
        VkImage image = VK_NULL_HANDLE;
        CHECK_EQ(vkCreateImage(vulkan.device(), &described.info(), nullptr, &image), VK_SUCCESS);
        VkMemoryRequirements needs = {};
        vkGetImageMemoryRequirements(vulkan.device(), image, &needs);
        /* The image fits in what Direct3D exported, or the bind below reads
         * past the texture. */
        CHECK(needs.size <= static_cast<VkDeviceSize>(bytes));
        const std::uint32_t type = vulkan.type_in(vulkan.dma_buf_types(fd) & needs.memoryTypeBits, 0u);
        CHECK(type < 32u);
        VkImportMemoryFdInfoKHR imported = {};
        imported.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        imported.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        imported.fd = fd;
        VkMemoryAllocateInfo allocate = {};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.pNext = &imported;
        allocate.allocationSize = static_cast<VkDeviceSize>(bytes);
        allocate.memoryTypeIndex = type;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        /* The driver owns the descriptor once the import succeeds. */
        CHECK_EQ(vkAllocateMemory(vulkan.device(), &allocate, nullptr, &memory), VK_SUCCESS);
        CHECK_EQ(vkBindImageMemory(vulkan.device(), image, memory, 0u), VK_SUCCESS);
        CHECK(vulkan.upload(image, pattern, sizeof pattern));

        static std::uint8_t pixels[texels * 4u];
        std::memset(pixels, 0, sizeof pixels);
        std::uint32_t row_pitch = 0u, depth_pitch = 0u;
        std::uint64_t read = 0u;
        CHECK(av_d3d_ReadSubresource(wire, resource, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof pixels,
                                      &read));
        CHECK_EQ(hresult, 0u);
        CHECK_EQ(row_pitch, width * 4u);
        /* Bytes as Vulkan stored them, in either channel order: a slot is
         * copied into, never converted. */
        const std::uint32_t probes[] = {0u, 1u, width - 1u, width, width * 3u + 17u, texels / 2u + 9u, texels - 1u};
        for (const std::uint32_t at : probes)
            for (std::uint32_t channel = 0u; channel < 4u; channel++)
                CHECK_EQ(pixels[at * 4u + channel], pattern[at * 4u + channel]);
        vkDestroyImage(vulkan.device(), image, nullptr);
        vkFreeMemory(vulkan.device(), memory, nullptr);
    }
    direct3d.shut_down();
}
