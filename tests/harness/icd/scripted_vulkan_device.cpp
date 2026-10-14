#include "harness/icd/scripted_vulkan_device.hpp"

#include <cstring>

namespace av {

namespace {

/* The bound device, as a loader's entry points reach a driver's root: the
 * entry points below are bare functions with nowhere to put a `this`. A test
 * may hold two devices at once, so the binding is a stack: the newest is the
 * bound one and its destruction gives the binding back. */
ScriptedVulkanDevice *bound = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL scripted_create_image(VkDevice, const VkImageCreateInfo *info,
                                                     const VkAllocationCallbacks *, VkImage *image)
{ return bound->make_image(info, image); }

VKAPI_ATTR void VKAPI_CALL scripted_destroy_image(VkDevice, VkImage image, const VkAllocationCallbacks *)
{ bound->destroy_image(image); }

VKAPI_ATTR void VKAPI_CALL scripted_image_requirements(VkDevice, VkImage image, VkMemoryRequirements *needs)
{ bound->image_requirements(image, needs); }

VKAPI_ATTR void VKAPI_CALL scripted_buffer_requirements(VkDevice, VkBuffer buffer, VkMemoryRequirements *needs)
{ bound->buffer_requirements(buffer, needs); }

VKAPI_ATTR void VKAPI_CALL scripted_memory_properties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *properties)
{ bound->memory_properties(properties); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_allocate(VkDevice, const VkMemoryAllocateInfo *info,
                                                 const VkAllocationCallbacks *, VkDeviceMemory *memory)
{ return bound->allocate(info, memory); }

VKAPI_ATTR void VKAPI_CALL scripted_free(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks *)
{ bound->free_memory(memory); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_bind_image(VkDevice, VkImage image, VkDeviceMemory memory, VkDeviceSize)
{ return bound->bind_image(image, memory); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_bind_buffer(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize)
{ return bound->bind_buffer(buffer, memory); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_create_buffer(VkDevice, const VkBufferCreateInfo *info,
                                                      const VkAllocationCallbacks *, VkBuffer *buffer)
{ return bound->make_buffer(info, buffer); }

VKAPI_ATTR void VKAPI_CALL scripted_destroy_buffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks *)
{ bound->destroy_buffer(buffer); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_map(VkDevice, VkDeviceMemory memory, VkDeviceSize, VkDeviceSize,
                                            VkMemoryMapFlags, void **view)
{ return bound->map(memory, view); }

VKAPI_ATTR void VKAPI_CALL scripted_unmap(VkDevice, VkDeviceMemory memory) { bound->unmap(memory); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_create_pool(VkDevice, const VkCommandPoolCreateInfo *info,
                                                    const VkAllocationCallbacks *, VkCommandPool *pool)
{ return bound->make_pool(info, pool); }

VKAPI_ATTR void VKAPI_CALL scripted_destroy_pool(VkDevice, VkCommandPool pool, const VkAllocationCallbacks *)
{ bound->destroy_pool(pool); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_allocate_commands(VkDevice, const VkCommandBufferAllocateInfo *info,
                                                          VkCommandBuffer *commands)
{ return bound->make_commands(info, commands); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_create_fence(VkDevice, const VkFenceCreateInfo *,
                                                     const VkAllocationCallbacks *, VkFence *fence)
{ return bound->make_fence(fence); }

VKAPI_ATTR void VKAPI_CALL scripted_destroy_fence(VkDevice, VkFence fence, const VkAllocationCallbacks *)
{ bound->destroy_fence(fence); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_reset_fences(VkDevice, std::uint32_t, const VkFence *)
{ return bound->reset_fences(); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_wait_fences(VkDevice, std::uint32_t, const VkFence *, VkBool32,
                                                    std::uint64_t timeout)
{ return bound->wait_fences(timeout); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_begin(VkCommandBuffer commands, const VkCommandBufferBeginInfo *)
{ return bound->begin(commands); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_end(VkCommandBuffer commands) { return bound->end(commands); }

VKAPI_ATTR void VKAPI_CALL scripted_barrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags,
                                            VkDependencyFlags, std::uint32_t, const VkMemoryBarrier *, std::uint32_t,
                                            const VkBufferMemoryBarrier *, std::uint32_t count,
                                            const VkImageMemoryBarrier *barriers)
{ bound->record_barrier(count, barriers); }

VKAPI_ATTR void VKAPI_CALL scripted_copy_to_buffer(VkCommandBuffer, VkImage image, VkImageLayout, VkBuffer,
                                                   std::uint32_t, const VkBufferImageCopy *region)
{ bound->record_copy(image, region); }

VKAPI_ATTR void VKAPI_CALL scripted_copy_image(VkCommandBuffer, VkImage source, VkImageLayout, VkImage target,
                                               VkImageLayout, std::uint32_t, const VkImageCopy *region)
{ bound->record_image_copy(source, target, region); }

VKAPI_ATTR VkResult VKAPI_CALL scripted_submit(VkQueue queue, std::uint32_t, const VkSubmitInfo *info, VkFence fence)
{ return bound->submit(queue, info, fence); }

struct Named {
    const char        *name;
    PFN_vkVoidFunction function;
};

const Named entries[] = {
    {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(&scripted_create_image)},
    {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(&scripted_destroy_image)},
    {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&scripted_image_requirements)},
    {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(&scripted_buffer_requirements)},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(&scripted_memory_properties)},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_allocate)},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_free)},
    {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_bind_image)},
    {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_bind_buffer)},
    {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(&scripted_create_buffer)},
    {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(&scripted_destroy_buffer)},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_map)},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(&scripted_unmap)},
    {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&scripted_create_pool)},
    {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&scripted_destroy_pool)},
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&scripted_allocate_commands)},
    {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(&scripted_create_fence)},
    {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(&scripted_destroy_fence)},
    {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(&scripted_reset_fences)},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(&scripted_wait_fences)},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&scripted_begin)},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(&scripted_end)},
    {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(&scripted_barrier)},
    {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(&scripted_copy_to_buffer)},
    {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(&scripted_copy_image)},
    {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(&scripted_submit)},
};

}  // namespace

ScriptedVulkanDevice::ScriptedVulkanDevice()
    : outer_(bound), refusal_{nullptr, 0u, VK_SUCCESS}, calls_(0u), images_{}, memories_{}, buffer_bytes_{}, buffer_size_(0u),
      device_local_(true), host_visible_(true), any_type_(true), overcount_(false), stall_(false), pool_made_(false),
      fence_made_(false), recording_(false), image_count_(0u), memory_count_(0u), mappings_(0u), pools_(0u), fences_(0u),
      submits_(0u), wait_count_(0u), wait_(VK_NULL_HANDLE), family_(0u), barriers_{}, barrier_count_(0u), copies_{},
      copy_count_(0u), image_copies_{}, image_copy_count_(0u)
{
    bound = this;
}

ScriptedVulkanDevice::~ScriptedVulkanDevice() { bound = outer_; }

PFN_vkVoidFunction ScriptedVulkanDevice::entry(const char *name)
{
    for (const Named &named : entries)
        if (std::strcmp(named.name, name) == 0) return named.function;
    return nullptr;
}

VkDevice ScriptedVulkanDevice::device() { return reinterpret_cast<VkDevice>(this); }

VkPhysicalDevice ScriptedVulkanDevice::physical() { return reinterpret_cast<VkPhysicalDevice>(&memories_[0]); }

VkQueue ScriptedVulkanDevice::queue() { return reinterpret_cast<VkQueue>(&buffer_bytes_[0]); }

void ScriptedVulkanDevice::refuse(const char *name, std::uint32_t nth, VkResult code)
{
    refusal_ = Refusal{name, nth, code};
    calls_ = 0u;
}

void ScriptedVulkanDevice::without_device_local() { device_local_ = false; }
void ScriptedVulkanDevice::without_host_visible() { host_visible_ = false; }
void ScriptedVulkanDevice::allow_no_memory_type() { any_type_ = false; }
void ScriptedVulkanDevice::overcount_memory_types() { overcount_ = true; }
void ScriptedVulkanDevice::stall_copy() { stall_ = true; }

VkResult ScriptedVulkanDevice::judge(const char *name)
{
    if (refusal_.name == nullptr || std::strcmp(refusal_.name, name) != 0) return VK_SUCCESS;
    calls_++;
    return calls_ == refusal_.nth ? refusal_.code : VK_SUCCESS;
}

ScriptedVulkanDevice::Image *ScriptedVulkanDevice::image_of(VkImage image)
{
    for (Image &slot : images_)
        if (static_cast<const void *>(image) == &slot && slot.made) return &slot;
    return nullptr;
}

const ScriptedVulkanDevice::Image *ScriptedVulkanDevice::image_of(VkImage image) const
{
    for (const Image &slot : images_)
        if (static_cast<const void *>(image) == &slot && slot.made) return &slot;
    return nullptr;
}

VkResult ScriptedVulkanDevice::make_image(const VkImageCreateInfo *info, VkImage *image)
{
    const VkResult refused = judge("vkCreateImage");
    if (refused != VK_SUCCESS) return refused;
    for (Image &slot : images_) {
        if (slot.made) continue;
        slot.made = true;
        slot.width = info->extent.width;
        slot.height = info->extent.height;
        slot.described = Described{info->format, info->flags, info->tiling, info->usage, 0u, 0u, 0u, ~0ull, VK_NULL_HANDLE};
        for (auto *node = static_cast<const VkBaseInStructure *>(info->pNext); node != nullptr; node = node->pNext) {
            if (node->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)
                slot.described.external = reinterpret_cast<const VkExternalMemoryImageCreateInfo *>(node)->handleTypes;
            if (node->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO)
                slot.described.view_formats =
                    reinterpret_cast<const VkImageFormatListCreateInfo *>(node)->viewFormatCount;
            if (node->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT) {
                const auto *listed = reinterpret_cast<const VkImageDrmFormatModifierListCreateInfoEXT *>(node);
                slot.described.modifiers = listed->drmFormatModifierCount;
                if (listed->drmFormatModifierCount != 0u) slot.described.first_modifier = listed->pDrmFormatModifiers[0];
            }
        }
        image_count_++;
        *image = reinterpret_cast<VkImage>(&slot);
        return VK_SUCCESS;
    }
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void ScriptedVulkanDevice::destroy_image(VkImage image)
{
    Image *const slot = image_of(image);
    if (slot == nullptr) return;
    slot->made = false;
    image_count_--;
}

void ScriptedVulkanDevice::image_requirements(VkImage image, VkMemoryRequirements *needs)
{
    const Image *const slot = image_of(image);
    needs->size = slot == nullptr ? 0u : VkDeviceSize{slot->width} * slot->height * 4u;
    needs->alignment = 16u;
    needs->memoryTypeBits = any_type_ ? 0x3u : 0u;
}

void ScriptedVulkanDevice::buffer_requirements(VkBuffer, VkMemoryRequirements *needs)
{
    needs->size = buffer_size_;
    needs->alignment = 16u;
    needs->memoryTypeBits = any_type_ ? 0x3u : 0u;
}

void ScriptedVulkanDevice::memory_properties(VkPhysicalDeviceMemoryProperties *properties)
{
    *properties = VkPhysicalDeviceMemoryProperties{};
    properties->memoryHeapCount = 1u;
    properties->memoryHeaps[0].size = 1u << 30u;
    if (device_local_) {
        properties->memoryTypes[properties->memoryTypeCount].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        properties->memoryTypeCount++;
    }
    if (host_visible_) {
        properties->memoryTypes[properties->memoryTypeCount].propertyFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        properties->memoryTypeCount++;
    }
    if (overcount_) properties->memoryTypeCount = VK_MAX_MEMORY_TYPES + 8u;
}

VkResult ScriptedVulkanDevice::allocate(const VkMemoryAllocateInfo *info, VkDeviceMemory *memory)
{
    const VkResult refused = judge("vkAllocateMemory");
    if (refused != VK_SUCCESS) return refused;
    for (Memory &slot : memories_) {
        if (slot.made) continue;
        slot.made = true;
        slot.size = info->allocationSize;
        slot.type = info->memoryTypeIndex;
        memory_count_++;
        *memory = reinterpret_cast<VkDeviceMemory>(&slot);
        return VK_SUCCESS;
    }
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void ScriptedVulkanDevice::free_memory(VkDeviceMemory memory)
{
    for (Memory &slot : memories_) {
        if (static_cast<const void *>(memory) != &slot || !slot.made) continue;
        slot.made = false;
        memory_count_--;
        return;
    }
}

VkResult ScriptedVulkanDevice::bind_image(VkImage image, VkDeviceMemory memory)
{
    const VkResult refused = judge("vkBindImageMemory");
    if (refused != VK_SUCCESS) return refused;
    Image *const slot = image_of(image);
    if (slot == nullptr) return VK_ERROR_UNKNOWN;
    slot->described.bound = memory;
    return VK_SUCCESS;
}

VkResult ScriptedVulkanDevice::bind_buffer(VkBuffer, VkDeviceMemory) { return judge("vkBindBufferMemory"); }

VkResult ScriptedVulkanDevice::make_buffer(const VkBufferCreateInfo *info, VkBuffer *buffer)
{
    const VkResult refused = judge("vkCreateBuffer");
    if (refused != VK_SUCCESS) return refused;
    if (info->size > max_image_bytes) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    buffer_size_ = info->size;
    *buffer = reinterpret_cast<VkBuffer>(&buffer_size_);
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::destroy_buffer(VkBuffer) {}

VkResult ScriptedVulkanDevice::map(VkDeviceMemory, void **view)
{
    const VkResult refused = judge("vkMapMemory");
    if (refused != VK_SUCCESS) return refused;
    mappings_++;
    *view = &buffer_bytes_[0];
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::unmap(VkDeviceMemory) { mappings_--; }

VkResult ScriptedVulkanDevice::make_pool(const VkCommandPoolCreateInfo *info, VkCommandPool *pool)
{
    const VkResult refused = judge("vkCreateCommandPool");
    if (refused != VK_SUCCESS) return refused;
    family_ = info->queueFamilyIndex;
    pool_made_ = true;
    pools_++;
    *pool = reinterpret_cast<VkCommandPool>(&pool_made_);
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::destroy_pool(VkCommandPool)
{
    pool_made_ = false;
    pools_--;
}

VkResult ScriptedVulkanDevice::make_commands(const VkCommandBufferAllocateInfo *, VkCommandBuffer *commands)
{
    const VkResult refused = judge("vkAllocateCommandBuffers");
    if (refused != VK_SUCCESS) return refused;
    *commands = reinterpret_cast<VkCommandBuffer>(&recording_);
    return VK_SUCCESS;
}

VkResult ScriptedVulkanDevice::make_fence(VkFence *fence)
{
    const VkResult refused = judge("vkCreateFence");
    if (refused != VK_SUCCESS) return refused;
    fence_made_ = true;
    fences_++;
    *fence = reinterpret_cast<VkFence>(&fence_made_);
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::destroy_fence(VkFence)
{
    fence_made_ = false;
    fences_--;
}

VkResult ScriptedVulkanDevice::reset_fences() { return judge("vkResetFences"); }

VkResult ScriptedVulkanDevice::wait_fences(std::uint64_t timeout)
{
    const VkResult refused = judge("vkWaitForFences");
    if (refused != VK_SUCCESS) return refused;
    return stall_ && timeout != UINT64_MAX ? VK_TIMEOUT : VK_SUCCESS;
}

VkResult ScriptedVulkanDevice::begin(VkCommandBuffer)
{
    const VkResult refused = judge("vkBeginCommandBuffer");
    if (refused != VK_SUCCESS) return refused;
    recording_ = true;
    barrier_count_ = 0u;
    copy_count_ = 0u;
    image_copy_count_ = 0u;
    return VK_SUCCESS;
}

VkResult ScriptedVulkanDevice::end(VkCommandBuffer)
{
    const VkResult refused = judge("vkEndCommandBuffer");
    if (refused != VK_SUCCESS) return refused;
    recording_ = false;
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::record_barrier(std::uint32_t count, const VkImageMemoryBarrier *barriers)
{
    for (std::uint32_t index = 0u; index < count && barrier_count_ < max_barriers; index++) {
        barriers_[barrier_count_] = Barrier{barriers[index].image, barriers[index].oldLayout,
                                            barriers[index].newLayout, barriers[index].dstQueueFamilyIndex};
        barrier_count_++;
    }
}

void ScriptedVulkanDevice::record_copy(VkImage image, const VkBufferImageCopy *region)
{
    if (copy_count_ == max_barriers) return;
    copies_[copy_count_] = Copy{image, region->imageExtent};
    copy_count_++;
}

void ScriptedVulkanDevice::record_image_copy(VkImage source, VkImage target, const VkImageCopy *region)
{
    if (image_copy_count_ == max_barriers) return;
    image_copies_[image_copy_count_] = ImageCopy{source, target, region->extent};
    image_copy_count_++;
}

/* A submit runs what was recorded: a copy to the buffer moves the image's
 * bytes into the read-back buffer, and one to an image into that image, as
 * the host's would. */
VkResult ScriptedVulkanDevice::submit(VkQueue, const VkSubmitInfo *info, VkFence)
{
    const VkResult refused = judge("vkQueueSubmit");
    if (refused != VK_SUCCESS) return refused;
    submits_++;
    wait_count_ = info->waitSemaphoreCount;
    wait_ = info->waitSemaphoreCount != 0u ? info->pWaitSemaphores[0] : VK_NULL_HANDLE;
    for (std::uint32_t index = 0u; index < copy_count_; index++) {
        const Image *const slot = image_of(copies_[index].image);
        if (slot == nullptr) continue;
        const std::uint32_t bytes = copies_[index].extent.width * copies_[index].extent.height * 4u;
        std::memcpy(buffer_bytes_, slot->bytes, bytes < max_image_bytes ? bytes : max_image_bytes);
    }
    for (std::uint32_t index = 0u; index < image_copy_count_; index++) {
        const Image *const source = image_of(image_copies_[index].source);
        Image *const       target = image_of(image_copies_[index].target);
        if (source == nullptr || target == nullptr) continue;
        const std::uint32_t bytes = image_copies_[index].extent.width * image_copies_[index].extent.height * 4u;
        std::memcpy(target->bytes, source->bytes, bytes < max_image_bytes ? bytes : max_image_bytes);
    }
    return VK_SUCCESS;
}

void ScriptedVulkanDevice::paint(VkImage image, std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                                 std::uint8_t alpha)
{
    Image *const slot = image_of(image);
    if (slot == nullptr) return;
    const std::uint8_t pixel[4] = {red, green, blue, alpha};
    for (std::uint32_t at = 0u; at + 4u <= max_image_bytes; at += 4u) std::memcpy(&slot->bytes[at], pixel, 4u);
}

void ScriptedVulkanDevice::paint_row(VkImage image, std::uint32_t row, std::uint32_t row_bytes, std::uint8_t red,
                                     std::uint8_t green, std::uint8_t blue, std::uint8_t alpha)
{
    Image *const slot = image_of(image);
    if (slot == nullptr) return;
    const std::uint8_t pixel[4] = {red, green, blue, alpha};
    const std::uint32_t first = row * row_bytes;
    for (std::uint32_t at = first; at + 4u <= first + row_bytes && at + 4u <= max_image_bytes; at += 4u)
        std::memcpy(&slot->bytes[at], pixel, 4u);
}

std::uint32_t ScriptedVulkanDevice::images() const { return image_count_; }
std::uint32_t ScriptedVulkanDevice::memories() const { return memory_count_; }
std::uint32_t ScriptedVulkanDevice::submits() const { return submits_; }
std::uint32_t ScriptedVulkanDevice::last_wait_count() const { return wait_count_; }
VkSemaphore   ScriptedVulkanDevice::last_wait() const { return wait_; }
std::uint32_t ScriptedVulkanDevice::barriers() const { return barrier_count_; }
std::uint32_t ScriptedVulkanDevice::copies() const { return copy_count_; }
std::uint32_t ScriptedVulkanDevice::image_copies() const { return image_copy_count_; }

ScriptedVulkanDevice::Described ScriptedVulkanDevice::described(VkImage image) const
{
    const Image *const slot = image_of(image);
    return slot != nullptr ? slot->described : Described{};
}

const std::uint8_t *ScriptedVulkanDevice::bytes(VkImage image) const
{
    const Image *const slot = image_of(image);
    return slot != nullptr ? slot->bytes : nullptr;
}
std::uint32_t ScriptedVulkanDevice::mappings() const { return mappings_; }
std::uint32_t ScriptedVulkanDevice::pools() const { return pools_; }
std::uint32_t ScriptedVulkanDevice::pool_family() const { return family_; }
std::uint32_t ScriptedVulkanDevice::fences() const { return fences_; }

const ScriptedVulkanDevice::Barrier &ScriptedVulkanDevice::barrier(std::uint32_t index) const
{
    return barriers_[index < max_barriers ? index : max_barriers - 1u];
}

const std::uint8_t *ScriptedVulkanDevice::readback() const { return buffer_bytes_; }

}  // namespace av
