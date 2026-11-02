#include <vulkan/vulkan.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(call)                                                                                  \
    do {                                                                                             \
        VkResult checked_ = (call);                                                                  \
        if (checked_ != VK_SUCCESS) {                                                                \
            fprintf(stderr, "%s:%d: %s answered %d\n", __FILE__, __LINE__, #call, (int)checked_);   \
            exit(1);                                                                                 \
        }                                                                                            \
    } while (0)

enum layout { optimal, rows, tiled, layouts };
static const char *const names[layouts] = {"optimal (DXVK's own)", "rows (linear, make_rows)", "tiled (make_tiled)"};

struct gpu {
    VkPhysicalDevice                 physical;
    VkDevice                         device;
    VkQueue                          queue;
    uint32_t                         family;
    VkPhysicalDeviceMemoryProperties memory;
    VkCommandPool                    pool;
    VkQueryPool                      stamps;
    double                           period;
    uint64_t                         tilings[64];
    uint32_t                         tiling_count;
};

struct image {
    VkImage        image;
    VkDeviceMemory memory;
    uint64_t       modifier;
};

static uint32_t width = 2560u, height = 1440u, passes = 60u;

static uint32_t memory_type(const struct gpu *gpu, uint32_t allowed, VkMemoryPropertyFlags wanted)
{
    const VkMemoryPropertyFlags tries[] = {wanted, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0u};
    for (uint32_t step = 0u; step < 3u; step++)
        for (uint32_t type = 0u; type < gpu->memory.memoryTypeCount; type++)
            if ((allowed & (1u << type)) != 0u &&
                (gpu->memory.memoryTypes[type].propertyFlags & tries[step]) == tries[step])
                return type;
    return UINT32_MAX;
}

static void list_tilings(struct gpu *gpu)
{
    VkDrmFormatModifierPropertiesEXT     found[64];
    VkDrmFormatModifierPropertiesListEXT listed = {VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT, NULL, 0u,
                                                   NULL};
    VkFormatProperties2 properties = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &listed, {0}};
    vkGetPhysicalDeviceFormatProperties2(gpu->physical, VK_FORMAT_B8G8R8A8_UNORM, &properties);
    listed.drmFormatModifierCount = listed.drmFormatModifierCount < 64u ? listed.drmFormatModifierCount : 64u;
    listed.pDrmFormatModifierProperties = found;
    vkGetPhysicalDeviceFormatProperties2(gpu->physical, VK_FORMAT_B8G8R8A8_UNORM, &properties);
    const VkFormatFeatureFlags drawn = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                       VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                       VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                       VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    const VkFormatFeatureFlags wanted = properties.formatProperties.optimalTilingFeatures & drawn;
    for (uint32_t index = 0u; index < listed.drmFormatModifierCount; index++)
        if (found[index].drmFormatModifier != 0u && found[index].drmFormatModifierPlaneCount == 1u &&
            (found[index].drmFormatModifierTilingFeatures & wanted) == wanted)
            gpu->tilings[gpu->tiling_count++] = found[index].drmFormatModifier;
}

static int make(struct gpu *gpu, enum layout kind, struct image *made)
{
    VkFormat                    views[2] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB};
    VkImageFormatListCreateInfo list = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, NULL, 2u, views};
    VkExternalMemoryImageCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, &list,
                                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkSubresourceLayout plane = {0u, 0u, ((uint64_t)width * 4u + 255u) & ~(uint64_t)255u, 0u, 0u};
    VkImageDrmFormatModifierExplicitCreateInfoEXT linear = {
        VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT, NULL, 0u, 1u, &plane};
    VkImageDrmFormatModifierListCreateInfoEXT listed = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
                                                        NULL, gpu->tiling_count, gpu->tilings};
    VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_B8G8R8A8_UNORM;
    info.extent = (VkExtent3D){width, height, 1u};
    info.mipLevels = 1u;
    info.arrayLayers = 1u;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.pNext = &list;
    if (kind != optimal) {
        if (kind == tiled && gpu->tiling_count == 0u) return 0;
        info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        list.pNext = kind == rows ? (const void *)&linear : (const void *)&listed;
        info.pNext = &external;
    }
    memset(made, 0, sizeof *made);
    if (vkCreateImage(gpu->device, &info, NULL, &made->image) != VK_SUCCESS) return 0;
    VkMemoryRequirements needs;
    vkGetImageMemoryRequirements(gpu->device, made->image, &needs);
    const VkMemoryPropertyFlags wanted = kind == rows
                                             ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                             : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, NULL, made->image,
                                               VK_NULL_HANDLE};
    VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, &dedicated,
                                           VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkMemoryAllocateInfo allocated = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, kind == optimal ? NULL : &exported,
                                      needs.size, memory_type(gpu, needs.memoryTypeBits, wanted)};
    CHECK(vkAllocateMemory(gpu->device, &allocated, NULL, &made->memory));
    CHECK(vkBindImageMemory(gpu->device, made->image, made->memory, 0u));
    if (kind != optimal) {
        PFN_vkGetImageDrmFormatModifierPropertiesEXT modifier_of = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
            vkGetDeviceProcAddr(gpu->device, "vkGetImageDrmFormatModifierPropertiesEXT");
        VkImageDrmFormatModifierPropertiesEXT laid = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT};
        CHECK(modifier_of(gpu->device, made->image, &laid));
        made->modifier = laid.drmFormatModifier;
    }
    return 1;
}

static void transition(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to)
{
    const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_MEMORY_WRITE_BIT,
                                          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, from, to,
                                          VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
                                          {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 0u, NULL,
                         0u, NULL, 1u, &barrier);
}

static double time_passes(struct gpu *gpu, const struct image *target, const struct image *source, int drawn)
{
    VkCommandBuffer                   commands;
    const VkCommandBufferAllocateInfo allocate = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, gpu->pool,
                                                  VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1u};
    CHECK(vkAllocateCommandBuffers(gpu->device, &allocate, &commands));
    const VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(commands, &begin));
    const VkClearColorValue       colour = {{0.2f, 0.4f, 0.6f, 1.0f}};
    const VkImageSubresourceRange whole = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    transition(commands, source->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdClearColorImage(commands, source->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colour, 1u, &whole);
    transition(commands, source->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(commands, target->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdResetQueryPool(commands, gpu->stamps, 0u, 2u);
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu->stamps, 0u);
    const VkImageSubresourceLayers layer = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    for (uint32_t pass = 0u; pass < passes; pass++) {
        if (drawn) {
            const VkImageBlit blit = {layer, {{0, 0, 0}, {(int32_t)width - 7, (int32_t)height - 5, 1}}, layer,
                                      {{0, 0, 0}, {(int32_t)width, (int32_t)height, 1}}};
            vkCmdBlitImage(commands, source->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &blit, VK_FILTER_LINEAR);
        } else {
            const VkImageCopy copy = {layer, {0, 0, 0}, layer, {0, 0, 0}, {width, height, 1u}};
            vkCmdCopyImage(commands, source->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy);
        }
        transition(commands, target->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu->stamps, 1u);
    CHECK(vkEndCommandBuffer(commands));
    const VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0u, NULL, NULL, 1u, &commands};
    CHECK(vkQueueSubmit(gpu->queue, 1u, &submit, VK_NULL_HANDLE));
    CHECK(vkQueueWaitIdle(gpu->queue));
    uint64_t stamps[2];
    CHECK(vkGetQueryPoolResults(gpu->device, gpu->stamps, 0u, 2u, sizeof stamps, stamps, sizeof stamps[0],
                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    vkFreeCommandBuffers(gpu->device, gpu->pool, 1u, &commands);
    return (double)(stamps[1] - stamps[0]) * gpu->period / 1e6 / passes;
}

static double best_of_three(struct gpu *gpu, const struct image *target, const struct image *source, int drawn)
{
    double best = 1e30;
    for (int run = 0; run < 4; run++) {
        const double taken = time_passes(gpu, target, source, drawn);
        if (run > 0 && taken < best) best = taken;
    }
    return best;
}

static int open_gpu(struct gpu *gpu, const char *wanted)
{
    const VkApplicationInfo    application = {VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "shared_image_bench", 1u, NULL,
                                              0u, VK_API_VERSION_1_2};
    const VkInstanceCreateInfo created = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0u, &application};
    VkInstance                 instance;
    CHECK(vkCreateInstance(&created, NULL, &instance));
    VkPhysicalDevice physicals[16];
    uint32_t         count = 16u;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, physicals));
    VkPhysicalDeviceProperties properties;
    memset(gpu, 0, sizeof *gpu);
    for (uint32_t index = 0u; index < count && gpu->physical == VK_NULL_HANDLE; index++) {
        vkGetPhysicalDeviceProperties(physicals[index], &properties);
        if (strstr(properties.deviceName, wanted) != NULL) gpu->physical = physicals[index];
    }
    if (gpu->physical == VK_NULL_HANDLE) return 0;
    vkGetPhysicalDeviceProperties(gpu->physical, &properties);
    gpu->period = properties.limits.timestampPeriod;
    printf("%s, %ux%u B8G8R8A8, %u passes a run\n", properties.deviceName, width, height, passes);
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &gpu->memory);
    VkQueueFamilyProperties families[16];
    uint32_t                family_count = 16u;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &family_count, families);
    while (gpu->family < family_count && (families[gpu->family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) gpu->family++;
    const float                   priority = 1.0f;
    const VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0u, gpu->family, 1u,
                                           &priority};
    const char *const extensions[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf",
                                      "VK_EXT_image_drm_format_modifier", "VK_KHR_image_format_list"};
    const VkDeviceCreateInfo device = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0u, 1u, &queue, 0u, NULL, 4u,
                                       extensions};
    CHECK(vkCreateDevice(gpu->physical, &device, NULL, &gpu->device));
    vkGetDeviceQueue(gpu->device, gpu->family, 0u, &gpu->queue);
    const VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0u, gpu->family};
    CHECK(vkCreateCommandPool(gpu->device, &pool, NULL, &gpu->pool));
    const VkQueryPoolCreateInfo stamps = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL, 0u, VK_QUERY_TYPE_TIMESTAMP, 2u};
    CHECK(vkCreateQueryPool(gpu->device, &stamps, NULL, &gpu->stamps));
    list_tilings(gpu);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc > 4) {
        width = (uint32_t)atoi(argv[2]);
        height = (uint32_t)atoi(argv[3]);
        passes = (uint32_t)atoi(argv[4]);
    }
    struct gpu gpu;
    if (!open_gpu(&gpu, argc > 1 ? argv[1] : "")) {
        fprintf(stderr, "no such GPU\n");
        return 1;
    }
    struct image plain;
    if (!make(&gpu, optimal, &plain)) return 1;
    for (int kind = optimal; kind < layouts; kind++) {
        struct image shared;
        if (!make(&gpu, (enum layout)kind, &shared)) {
            printf("%-26s not made here\n", names[kind]);
            continue;
        }
        const double into = best_of_three(&gpu, &shared, &plain, 1);
        const double from = best_of_three(&gpu, &plain, &shared, 1);
        const double copied = best_of_three(&gpu, &shared, &plain, 0);
        printf("%-26s modifier %016llx: drawn into %6.3f ms, drawn from %6.3f ms, copied into %6.3f ms\n", names[kind],
               (unsigned long long)shared.modifier, into, from, copied);
        vkDestroyImage(gpu.device, shared.image, NULL);
        vkFreeMemory(gpu.device, shared.memory, NULL);
    }
    return 0;
}
