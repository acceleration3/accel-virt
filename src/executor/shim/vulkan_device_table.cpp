#include "executor/shim/vulkan_device_table.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"

#include <vulkan/vulkan.h>

namespace av {

namespace {

constexpr uint64_t drm_format_mod_linear = 0u;
constexpr uint64_t drm_format_mod_invalid = 0x00ffffffffffffffull;

}

VulkanDeviceTable::VulkanDeviceTable()
    : physical_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), memory_{}, format_properties_(nullptr),
      format_features_(nullptr), image_modifier_(nullptr),
      create_image_(nullptr), destroy_image_(nullptr), requirements_(nullptr), allocate_(nullptr), free_(nullptr), map_(nullptr),
      bind_(nullptr), layout_(nullptr), memory_fd_(nullptr), create_semaphore_(nullptr), destroy_semaphore_(nullptr),
      import_semaphore_(nullptr), semaphore_fd_(nullptr), wait_semaphores_(nullptr), create_pool_(nullptr),
      destroy_pool_(nullptr), allocate_buffers_(nullptr), begin_(nullptr), end_(nullptr), reset_(nullptr),
      barrier_(nullptr), copy_image_(nullptr), submit_(nullptr), queue_idle_(nullptr), lose_(nullptr)
{
    AV_SCOPE("VulkanDeviceTable::VulkanDeviceTable");
}

bool VulkanDeviceTable::load(VkInstance instance, VkPhysicalDevice physical, VkDevice device)
{
    AV_SCOPE("VulkanDeviceTable::load");
    forget();
    if (instance == VK_NULL_HANDLE || physical == VK_NULL_HANDLE || device == VK_NULL_HANDLE) return false;
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    format_properties_ = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2"));
    format_features_ = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2"));
    if (properties == nullptr) return false;
    properties(physical, &memory_);
    const auto entry = [device](const char *name) { return vkGetDeviceProcAddr(device, name); };
    create_image_ = reinterpret_cast<PFN_vkCreateImage>(entry("vkCreateImage"));
    destroy_image_ = reinterpret_cast<PFN_vkDestroyImage>(entry("vkDestroyImage"));
    requirements_ = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(entry("vkGetImageMemoryRequirements"));
    allocate_ = reinterpret_cast<PFN_vkAllocateMemory>(entry("vkAllocateMemory"));
    free_ = reinterpret_cast<PFN_vkFreeMemory>(entry("vkFreeMemory"));
    map_ = reinterpret_cast<PFN_vkMapMemory>(entry("vkMapMemory"));
    bind_ = reinterpret_cast<PFN_vkBindImageMemory>(entry("vkBindImageMemory"));
    layout_ = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(entry("vkGetImageSubresourceLayout"));
    image_modifier_ = reinterpret_cast<PFN_vkGetImageDrmFormatModifierPropertiesEXT>(
        entry("vkGetImageDrmFormatModifierPropertiesEXT"));
    memory_fd_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(entry("vkGetMemoryFdKHR"));
    create_buffer_ = reinterpret_cast<PFN_vkCreateBuffer>(entry("vkCreateBuffer"));
    destroy_buffer_ = reinterpret_cast<PFN_vkDestroyBuffer>(entry("vkDestroyBuffer"));
    buffer_requirements_ =
        reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(entry("vkGetBufferMemoryRequirements"));
    bind_buffer_ = reinterpret_cast<PFN_vkBindBufferMemory>(entry("vkBindBufferMemory"));
    fd_properties_ = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(entry("vkGetMemoryFdPropertiesKHR"));
    create_semaphore_ = reinterpret_cast<PFN_vkCreateSemaphore>(entry("vkCreateSemaphore"));
    destroy_semaphore_ = reinterpret_cast<PFN_vkDestroySemaphore>(entry("vkDestroySemaphore"));
    import_semaphore_ = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(entry("vkImportSemaphoreFdKHR"));
    semaphore_fd_ = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(entry("vkGetSemaphoreFdKHR"));
    wait_semaphores_ = reinterpret_cast<PFN_vkWaitSemaphores>(entry("vkWaitSemaphores"));
    create_pool_ = reinterpret_cast<PFN_vkCreateCommandPool>(entry("vkCreateCommandPool"));
    destroy_pool_ = reinterpret_cast<PFN_vkDestroyCommandPool>(entry("vkDestroyCommandPool"));
    allocate_buffers_ = reinterpret_cast<PFN_vkAllocateCommandBuffers>(entry("vkAllocateCommandBuffers"));
    begin_ = reinterpret_cast<PFN_vkBeginCommandBuffer>(entry("vkBeginCommandBuffer"));
    end_ = reinterpret_cast<PFN_vkEndCommandBuffer>(entry("vkEndCommandBuffer"));
    reset_ = reinterpret_cast<PFN_vkResetCommandBuffer>(entry("vkResetCommandBuffer"));
    barrier_ = reinterpret_cast<PFN_vkCmdPipelineBarrier>(entry("vkCmdPipelineBarrier"));
    copy_image_ = reinterpret_cast<PFN_vkCmdCopyImage>(entry("vkCmdCopyImage"));
    submit_ = reinterpret_cast<PFN_vkQueueSubmit>(entry("vkQueueSubmit"));
    queue_idle_ = reinterpret_cast<PFN_vkQueueWaitIdle>(entry("vkQueueWaitIdle"));
    lose_ = reinterpret_cast<void (*)(VkDevice)>(entry("vkAvLoseDeviceAV"));
    physical_ = physical;
    device_ = device;
    return true;
}

bool VulkanDeviceTable::interoperates() const
{
    AV_SCOPE("VulkanDeviceTable::interoperates");
    return loaded() && memory_fd_ != nullptr && import_semaphore_ != nullptr && semaphore_fd_ != nullptr &&
           wait_semaphores_ != nullptr;
}

bool VulkanDeviceTable::imports_dma_bufs() const
{
    AV_SCOPE("VulkanDeviceTable::imports_dma_bufs");
    return loaded() && fd_properties_ != nullptr;
}

void VulkanDeviceTable::forget()
{
    AV_SCOPE("VulkanDeviceTable::forget");
    physical_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

bool VulkanDeviceTable::lose() const
{
    AV_SCOPE("VulkanDeviceTable::lose");
    if (!loaded() || lose_ == nullptr) return false;
    lose_(device_);
    return true;
}

bool VulkanDeviceTable::loaded() const
{
    AV_SCOPE("VulkanDeviceTable::loaded");
    return device_ != VK_NULL_HANDLE;
}

VkDevice VulkanDeviceTable::device() const
{
    AV_SCOPE("VulkanDeviceTable::device");
    return device_;
}

bool VulkanDeviceTable::supports_exportable(const VkImageCreateInfo &info) const
{
    AV_SCOPE("VulkanDeviceTable::supports_exportable");
    if (!loaded() || format_properties_ == nullptr) return false;
    const VkImageFormatListCreateInfo *views = nullptr;
    const uint64_t                    *modifiers = nullptr;
    uint32_t                           count = 0u;
    for (auto *next = static_cast<const VkBaseInStructure *>(info.pNext); next != nullptr; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO)
            views = reinterpret_cast<const VkImageFormatListCreateInfo *>(next);
        if (next->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT) {
            modifiers = &reinterpret_cast<const VkImageDrmFormatModifierExplicitCreateInfoEXT *>(next)->drmFormatModifier;
            count = 1u;
        }
        if (next->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT) {
            const auto *listed = reinterpret_cast<const VkImageDrmFormatModifierListCreateInfoEXT *>(next);
            modifiers = listed->pDrmFormatModifiers;
            count = listed->drmFormatModifierCount;
        }
    }
    if (info.tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) return count == 0u && supports_layout(info, views, nullptr);
    if (modifiers == nullptr || count == 0u) return false;
    for (uint32_t index = 0u; index < count; index++)
        if (!supports_layout(info, views, &modifiers[index])) return false;
    return true;
}

bool VulkanDeviceTable::supports_layout(const VkImageCreateInfo &info, const VkImageFormatListCreateInfo *views,
                                        const uint64_t *modifier) const
{
    AV_SCOPE("VulkanDeviceTable::supports_layout");
    VkPhysicalDeviceExternalImageFormatInfo external = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
                                                        nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkImageFormatListCreateInfo listed = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, nullptr, 0u, nullptr};
    if (views != nullptr) {
        listed = *views;
        listed.pNext = external.pNext;
        external.pNext = &listed;
    }
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT laid = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT, nullptr, 0u, VK_SHARING_MODE_EXCLUSIVE, 0u,
        nullptr};
    if (modifier != nullptr) {
        laid.drmFormatModifier = *modifier;
        laid.pNext = external.pNext;
        external.pNext = &laid;
    }
    const VkPhysicalDeviceImageFormatInfo2 asked = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                                                    &external,
                                                    info.format,
                                                    info.imageType,
                                                    info.tiling,
                                                    info.usage,
                                                    info.flags};
    VkExternalImageFormatProperties exported = {VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES, nullptr, {}};
    VkImageFormatProperties2        answer = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, &exported, {}};
    if (format_properties_(physical_, &asked, &answer) != VK_SUCCESS) return false;
    const VkImageFormatProperties &limits = answer.imageFormatProperties;
    return info.extent.width <= limits.maxExtent.width && info.extent.height <= limits.maxExtent.height &&
           info.mipLevels <= limits.maxMipLevels && info.arrayLayers <= limits.maxArrayLayers &&
           (limits.sampleCounts & info.samples) != 0u &&
           (exported.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0u;
}

uint32_t VulkanDeviceTable::tiled_modifiers(const VkImageCreateInfo &rows, uint64_t *modifiers, uint32_t capacity) const
{
    AV_SCOPE("VulkanDeviceTable::tiled_modifiers");
    if (!loaded() || format_features_ == nullptr || image_modifier_ == nullptr) return 0u;
    constexpr uint32_t most_listed = 64u;
    VkDrmFormatModifierPropertiesEXT     found[most_listed] = {};
    VkDrmFormatModifierPropertiesListEXT listed = {VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT, nullptr, 0u,
                                                   nullptr};
    VkFormatProperties2 properties = {VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &listed, {}};
    format_features_(physical_, rows.format, &properties);
    listed.drmFormatModifierCount = listed.drmFormatModifierCount < most_listed ? listed.drmFormatModifierCount : most_listed;
    listed.pDrmFormatModifierProperties = found;
    format_features_(physical_, rows.format, &properties);
    constexpr VkFormatFeatureFlags drawn = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                           VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                           VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    const VkFormatFeatureFlags wanted = properties.formatProperties.optimalTilingFeatures & drawn;
    const VkImageFormatListCreateInfo *views = nullptr;
    for (auto *next = static_cast<const VkBaseInStructure *>(rows.pNext); next != nullptr; next = next->pNext)
        if (next->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO)
            views = reinterpret_cast<const VkImageFormatListCreateInfo *>(next);
    uint32_t count = 0u;
    for (uint32_t index = 0u; index < listed.drmFormatModifierCount && count < capacity; index++) {
        const VkDrmFormatModifierPropertiesEXT &one = found[index];
        if (one.drmFormatModifier == drm_format_mod_linear || one.drmFormatModifierPlaneCount != 1u ||
            (one.drmFormatModifierTilingFeatures & wanted) != wanted ||
            !supports_layout(rows, views, &one.drmFormatModifier))
            continue;
        modifiers[count++] = one.drmFormatModifier;
    }
    return count;
}

uint64_t VulkanDeviceTable::modifier_of(VkImage image) const
{
    AV_SCOPE("VulkanDeviceTable::modifier_of");
    VkImageDrmFormatModifierPropertiesEXT laid = {VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT, nullptr,
                                                  drm_format_mod_invalid};
    if (!loaded() || image_modifier_ == nullptr || image_modifier_(device_, image, &laid) != VK_SUCCESS)
        return drm_format_mod_invalid;
    return laid.drmFormatModifier;
}

uint32_t VulkanDeviceTable::memory_type(uint32_t allowed) const
{
    AV_SCOPE("VulkanDeviceTable::memory_type");
    for (uint32_t type = 0u; type < memory_.memoryTypeCount; type++)
        if ((allowed & (1u << type)) != 0u &&
            (memory_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u)
            return type;
    for (uint32_t type = 0u; type < memory_.memoryTypeCount; type++)
        if ((allowed & (1u << type)) != 0u) return type;
    return VK_MAX_MEMORY_TYPES;
}

uint32_t VulkanDeviceTable::raw_memory_type(uint32_t allowed) const
{
    AV_SCOPE("VulkanDeviceTable::raw_memory_type");
    constexpr VkMemoryPropertyFlags visible_vram =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    for (uint32_t type = 0u; type < memory_.memoryTypeCount; type++)
        if ((allowed & (1u << type)) != 0u && (memory_.memoryTypes[type].propertyFlags & visible_vram) == visible_vram)
            return type;
    return memory_type(allowed);
}

uint32_t VulkanDeviceTable::system_memory_type(uint32_t allowed) const
{
    AV_SCOPE("VulkanDeviceTable::system_memory_type");
    for (uint32_t type = 0u; type < memory_.memoryTypeCount; type++) {
        const VkMemoryPropertyFlags flags = memory_.memoryTypes[type].propertyFlags;
        if ((allowed & (1u << type)) != 0u && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u &&
            (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0u)
            return type;
    }
    return VK_MAX_MEMORY_TYPES;
}

void VulkanDeviceTable::memory_properties(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("VulkanDeviceTable::memory_properties");
    answer = loaded() ? memory_ : VkPhysicalDeviceMemoryProperties{};
}

bool VulkanDeviceTable::allocate_exportable(VkDeviceSize size, uint32_t type, VkDeviceMemory &memory) const
{
    AV_SCOPE("VulkanDeviceTable::allocate_exportable");
    memory = VK_NULL_HANDLE;
    if (!loaded()) return false;
    const VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkMemoryAllocateInfo       info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exported, size, type};
    if (allocate_(device_, &info, nullptr, &memory) == VK_SUCCESS) return true;
    memory = VK_NULL_HANDLE;
    return false;
}

bool VulkanDeviceTable::allocate_exportable(VkImage image, VkDeviceSize size, uint32_t type,
                                            VkDeviceMemory &memory) const
{
    AV_SCOPE("VulkanDeviceTable::allocate_exportable");
    memory = VK_NULL_HANDLE;
    if (!loaded() || type == VK_MAX_MEMORY_TYPES) return false;
    const VkExportMemoryAllocateInfo    exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, &exported, image,
                                                     VK_NULL_HANDLE};
    const VkMemoryAllocateInfo          info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &dedicated, size, type};
    if (allocate_(device_, &info, nullptr, &memory) == VK_SUCCESS) return true;
    memory = VK_NULL_HANDLE;
    return false;
}

bool VulkanDeviceTable::import_memory(OwnedFd &descriptor, VkExternalMemoryHandleTypeFlagBits handle,
                                      VkDeviceSize size, uint32_t type, const void *next, VkDeviceMemory &memory) const
{
    AV_SCOPE("VulkanDeviceTable::import_memory");
    memory = VK_NULL_HANDLE;
    if (!loaded() || !descriptor.held() || type >= memory_.memoryTypeCount) return false;
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, next, handle,
                                              descriptor.fd()};
    const VkMemoryAllocateInfo    info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &imported, size, type};
    const OwnedFdIdentity         before = descriptor.identity();
    if (allocate_(device_, &info, nullptr, &memory) != VK_SUCCESS) {
        memory = VK_NULL_HANDLE;
        descriptor.settle_failed_import(before);
        return false;
    }
    static_cast<void>(descriptor.take());
    return true;
}

bool VulkanDeviceTable::map_memory(VkDeviceMemory memory, void *&pointer) const
{
    AV_SCOPE("VulkanDeviceTable::map_memory");
    pointer = nullptr;
    if (!loaded() || map_ == nullptr || memory == VK_NULL_HANDLE) return false;
    if (map_(device_, memory, 0u, VK_WHOLE_SIZE, 0u, &pointer) == VK_SUCCESS) return true;
    pointer = nullptr;
    return false;
}

bool VulkanDeviceTable::create_image(const VkImageCreateInfo &info, VkImage &image) const
{
    AV_SCOPE("VulkanDeviceTable::create_image");
    image = VK_NULL_HANDLE;
    if (!loaded()) return false;
    if (create_image_(device_, &info, nullptr, &image) == VK_SUCCESS) return true;
    image = VK_NULL_HANDLE;
    return false;
}

void VulkanDeviceTable::destroy_image(VkImage image) const
{
    AV_SCOPE("VulkanDeviceTable::destroy_image");
    if (loaded() && image != VK_NULL_HANDLE) destroy_image_(device_, image, nullptr);
}

VkMemoryRequirements VulkanDeviceTable::requirements(VkImage image) const
{
    AV_SCOPE("VulkanDeviceTable::requirements");
    VkMemoryRequirements needs = {};
    if (loaded()) requirements_(device_, image, &needs);
    return needs;
}

bool VulkanDeviceTable::bind_image(VkImage image, VkDeviceMemory memory) const
{
    AV_SCOPE("VulkanDeviceTable::bind_image");
    return loaded() && image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE &&
           bind_(device_, image, memory, 0u) == VK_SUCCESS;
}

bool VulkanDeviceTable::create_buffer(const VkBufferCreateInfo &info, VkBuffer &buffer) const
{
    AV_SCOPE("VulkanDeviceTable::create_buffer");
    buffer = VK_NULL_HANDLE;
    if (!loaded() || create_buffer_ == nullptr) return false;
    if (create_buffer_(device_, &info, nullptr, &buffer) == VK_SUCCESS) return true;
    buffer = VK_NULL_HANDLE;
    return false;
}

void VulkanDeviceTable::destroy_buffer(VkBuffer buffer) const
{
    AV_SCOPE("VulkanDeviceTable::destroy_buffer");
    if (loaded() && buffer != VK_NULL_HANDLE) destroy_buffer_(device_, buffer, nullptr);
}

VkMemoryRequirements VulkanDeviceTable::buffer_requirements(VkBuffer buffer) const
{
    AV_SCOPE("VulkanDeviceTable::buffer_requirements");
    VkMemoryRequirements needed = {};
    if (loaded() && buffer != VK_NULL_HANDLE) buffer_requirements_(device_, buffer, &needed);
    return needed;
}

uint32_t VulkanDeviceTable::dma_buf_types(int fd) const
{
    AV_SCOPE("VulkanDeviceTable::dma_buf_types");
    if (!loaded() || fd_properties_ == nullptr || fd < 0) return 0u;
    VkMemoryFdPropertiesKHR properties = {VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR, nullptr, 0u};
    if (fd_properties_(device_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &properties) != VK_SUCCESS)
        return 0u;
    return properties.memoryTypeBits;
}

bool VulkanDeviceTable::bind_buffer(VkBuffer buffer, VkDeviceMemory memory) const
{
    AV_SCOPE("VulkanDeviceTable::bind_buffer");
    return loaded() && buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE &&
           bind_buffer_(device_, buffer, memory, 0u) == VK_SUCCESS;
}

void VulkanDeviceTable::free_memory(VkDeviceMemory memory) const
{
    AV_SCOPE("VulkanDeviceTable::free_memory");
    if (loaded() && memory != VK_NULL_HANDLE) free_(device_, memory, nullptr);
}

bool VulkanDeviceTable::memory_fd(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits type, int &fd) const
{
    AV_SCOPE("VulkanDeviceTable::memory_fd");
    fd = -1;
    if (!loaded() || memory_fd_ == nullptr || memory == VK_NULL_HANDLE) return false;
    const VkMemoryGetFdInfoKHR asked = {VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr, memory, type};
    if (memory_fd_(device_, &asked, &fd) == VK_SUCCESS) return true;
    fd = -1;
    return false;
}

VkSubresourceLayout VulkanDeviceTable::row_layout(VkImage image) const
{
    AV_SCOPE("VulkanDeviceTable::row_layout");
    const VkImageSubresource first = {VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT, 0u, 0u};
    VkSubresourceLayout      laid = {};
    if (loaded() && layout_ != nullptr) layout_(device_, image, &first, &laid);
    return laid;
}

bool VulkanDeviceTable::create_semaphore(const void *chain, VkSemaphore &semaphore) const
{
    AV_SCOPE("VulkanDeviceTable::create_semaphore");
    semaphore = VK_NULL_HANDLE;
    if (!loaded()) return false;
    const VkSemaphoreCreateInfo created = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, chain, 0u};
    if (create_semaphore_(device_, &created, nullptr, &semaphore) == VK_SUCCESS) return true;
    semaphore = VK_NULL_HANDLE;
    return false;
}

void VulkanDeviceTable::destroy_semaphore(VkSemaphore semaphore) const
{
    AV_SCOPE("VulkanDeviceTable::destroy_semaphore");
    if (loaded() && semaphore != VK_NULL_HANDLE) destroy_semaphore_(device_, semaphore, nullptr);
}

bool VulkanDeviceTable::import_semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                            int fd) const
{
    AV_SCOPE("VulkanDeviceTable::import_semaphore_fd");
    if (!loaded() || import_semaphore_ == nullptr) return false;
    const VkImportSemaphoreFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR, nullptr, semaphore,
                                                 0u, type, fd};
    return import_semaphore_(device_, &imported) == VK_SUCCESS;
}

bool VulkanDeviceTable::semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                     int &fd) const
{
    AV_SCOPE("VulkanDeviceTable::semaphore_fd");
    fd = -1;
    if (!loaded() || semaphore_fd_ == nullptr) return false;
    const VkSemaphoreGetFdInfoKHR asked = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, nullptr, semaphore, type};
    if (semaphore_fd_(device_, &asked, &fd) == VK_SUCCESS) return true;
    fd = -1;
    return false;
}

bool VulkanDeviceTable::wait_semaphore(VkSemaphore semaphore, uint64_t value, uint64_t nanoseconds) const
{
    AV_SCOPE("VulkanDeviceTable::wait_semaphore");
    if (!loaded() || wait_semaphores_ == nullptr) return false;
    const VkSemaphoreWaitInfo wait = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, nullptr, 0u, 1u, &semaphore, &value};
    return wait_semaphores_(device_, &wait, nanoseconds) == VK_SUCCESS;
}

bool VulkanDeviceTable::create_command_pool(uint32_t family, VkCommandPool &pool) const
{
    AV_SCOPE("VulkanDeviceTable::create_command_pool");
    pool = VK_NULL_HANDLE;
    if (!loaded()) return false;
    const VkCommandPoolCreateInfo created = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family};
    if (create_pool_(device_, &created, nullptr, &pool) == VK_SUCCESS) return true;
    pool = VK_NULL_HANDLE;
    return false;
}

void VulkanDeviceTable::destroy_command_pool(VkCommandPool pool) const
{
    AV_SCOPE("VulkanDeviceTable::destroy_command_pool");
    if (loaded() && pool != VK_NULL_HANDLE) destroy_pool_(device_, pool, nullptr);
}

bool VulkanDeviceTable::allocate_command_buffers(VkCommandPool pool, uint32_t count, VkCommandBuffer *buffers) const
{
    AV_SCOPE("VulkanDeviceTable::allocate_command_buffers");
    if (!loaded()) return false;
    const VkCommandBufferAllocateInfo allocation = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool,
                                                    VK_COMMAND_BUFFER_LEVEL_PRIMARY, count};
    return allocate_buffers_(device_, &allocation, buffers) == VK_SUCCESS;
}

bool VulkanDeviceTable::begin(VkCommandBuffer buffer) const
{
    AV_SCOPE("VulkanDeviceTable::begin");
    const VkCommandBufferBeginInfo begun = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    return loaded() && begin_(buffer, &begun) == VK_SUCCESS;
}

bool VulkanDeviceTable::end(VkCommandBuffer buffer) const
{
    AV_SCOPE("VulkanDeviceTable::end");
    return loaded() && end_(buffer) == VK_SUCCESS;
}

void VulkanDeviceTable::reset(VkCommandBuffer buffer) const
{
    AV_SCOPE("VulkanDeviceTable::reset");
    if (loaded()) reset_(buffer, 0u);
}

void VulkanDeviceTable::barrier(VkCommandBuffer buffer, VkPipelineStageFlags from, VkPipelineStageFlags to,
                                uint32_t count, const VkImageMemoryBarrier *barriers) const
{
    AV_SCOPE("VulkanDeviceTable::barrier");
    if (loaded()) barrier_(buffer, from, to, 0u, 0u, nullptr, 0u, nullptr, count, barriers);
}

void VulkanDeviceTable::copy_image(VkCommandBuffer buffer, VkImage source, VkImage destination,
                                   const VkImageCopy &region) const
{
    AV_SCOPE("VulkanDeviceTable::copy_image");
    if (loaded())
        copy_image_(buffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
}

bool VulkanDeviceTable::submit(VkQueue queue, const VkSubmitInfo &submitted) const
{
    AV_SCOPE("VulkanDeviceTable::submit");
    return loaded() && submit_(queue, 1u, &submitted, VK_NULL_HANDLE) == VK_SUCCESS;
}

void VulkanDeviceTable::queue_idle(VkQueue queue) const
{
    AV_SCOPE("VulkanDeviceTable::queue_idle");
    if (loaded()) static_cast<void>(queue_idle_(queue));
}

}
