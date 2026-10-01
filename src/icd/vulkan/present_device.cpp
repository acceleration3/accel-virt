#include "icd/vulkan/present_device.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/entry_lookup.hpp"

namespace av::icd {

namespace {

/* Where a present's semaphores are waited: the copy is a transfer. */
constexpr VkPipelineStageFlags present_stages[PresentDevice::max_waits] = {
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};

using CreateImage = VkResult(VKAPI_PTR *)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *);
using DestroyImage = void(VKAPI_PTR *)(VkDevice, VkImage, const VkAllocationCallbacks *);
using ImageRequirements = void(VKAPI_PTR *)(VkDevice, VkImage, VkMemoryRequirements *);
using BufferRequirements = void(VKAPI_PTR *)(VkDevice, VkBuffer, VkMemoryRequirements *);
using MemoryProperties = void(VKAPI_PTR *)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *);
using Allocate = VkResult(VKAPI_PTR *)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *,
                                       VkDeviceMemory *);
using Free = void(VKAPI_PTR *)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *);
using BindImage = VkResult(VKAPI_PTR *)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
using BindBuffer = VkResult(VKAPI_PTR *)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
using CreateBuffer = VkResult(VKAPI_PTR *)(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *,
                                           VkBuffer *);
using DestroyBuffer = void(VKAPI_PTR *)(VkDevice, VkBuffer, const VkAllocationCallbacks *);
using Map = VkResult(VKAPI_PTR *)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **);
using Unmap = void(VKAPI_PTR *)(VkDevice, VkDeviceMemory);
using CreatePool = VkResult(VKAPI_PTR *)(VkDevice, const VkCommandPoolCreateInfo *, const VkAllocationCallbacks *,
                                         VkCommandPool *);
using DestroyPool = void(VKAPI_PTR *)(VkDevice, VkCommandPool, const VkAllocationCallbacks *);
using AllocateCommands = VkResult(VKAPI_PTR *)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using CreateFence = VkResult(VKAPI_PTR *)(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *,
                                          VkFence *);
using DestroyFence = void(VKAPI_PTR *)(VkDevice, VkFence, const VkAllocationCallbacks *);
using ResetFences = VkResult(VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *);
using WaitFences = VkResult(VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t);
using Begin = VkResult(VKAPI_PTR *)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
using End = VkResult(VKAPI_PTR *)(VkCommandBuffer);
using Barrier = void(VKAPI_PTR *)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
                                  uint32_t, const VkMemoryBarrier *, uint32_t, const VkBufferMemoryBarrier *, uint32_t,
                                  const VkImageMemoryBarrier *);
using CopyToBuffer = void(VKAPI_PTR *)(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t,
                                       const VkBufferImageCopy *);
using CopyImage = void(VKAPI_PTR *)(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t,
                                    const VkImageCopy *);
using Submit = VkResult(VKAPI_PTR *)(VkQueue, uint32_t, const VkSubmitInfo *, VkFence);

constexpr uint32_t present_family = 0u;

}  // namespace

PresentDevice::PresentDevice(EntryLookup &entries, VkDevice device, VkPhysicalDevice physical)
    : entries_(entries), device_(device), physical_(physical), made_{}, image_count_(0u), buffer_(VK_NULL_HANDLE),
      buffer_memory_(VK_NULL_HANDLE), mapped_(nullptr), pool_(VK_NULL_HANDLE), commands_(VK_NULL_HANDLE),
      fence_(VK_NULL_HANDLE)
{
    AV_SCOPE("PresentDevice::PresentDevice");
}

PresentDevice::~PresentDevice()
{
    AV_SCOPE("PresentDevice::~PresentDevice");
    release();
}

template <typename Function>
Function PresentDevice::call(const char *name)
{
    AV_SCOPE("PresentDevice::call");
    return reinterpret_cast<Function>(entries_.entry(name));
}

VkResult PresentDevice::make_image(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImage &image)
{
    AV_SCOPE("PresentDevice::make_image");
    image = VK_NULL_HANDLE;
    if (image_count_ == max_images) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {extent.width, extent.height, 1u};
    info.mipLevels = 1u;
    info.arrayLayers = 1u;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    const VkResult made = call<CreateImage>("vkCreateImage")(device_, &info, nullptr, &image);
    if (made != VK_SUCCESS) {
        image = VK_NULL_HANDLE;
        return made;
    }
    PresentImage &slot = made_[image_count_];
    const VkResult bound = bind_image_memory(image, slot);
    if (bound != VK_SUCCESS) {
        call<DestroyImage>("vkDestroyImage")(device_, image, nullptr);
        image = VK_NULL_HANDLE;
        return bound;
    }
    slot.image_ = image;
    image_count_++;
    return VK_SUCCESS;
}

VkResult PresentDevice::bind_image_memory(VkImage image, PresentImage &made)
{
    AV_SCOPE("PresentDevice::bind_image_memory");
    VkMemoryRequirements needs = {};
    call<ImageRequirements>("vkGetImageMemoryRequirements")(device_, image, &needs);
    uint32_t type = 0u;
    if (!memory_type(needs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, type) &&
        !memory_type(needs.memoryTypeBits, 0u, type))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = needs.size;
    info.memoryTypeIndex = type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    const VkResult allocated = call<Allocate>("vkAllocateMemory")(device_, &info, nullptr, &memory);
    if (allocated != VK_SUCCESS) return allocated;
    const VkResult bound = call<BindImage>("vkBindImageMemory")(device_, image, memory, 0u);
    if (bound != VK_SUCCESS) {
        call<Free>("vkFreeMemory")(device_, memory, nullptr);
        return bound;
    }
    made.memory_ = memory;
    return VK_SUCCESS;
}

bool PresentDevice::memory_type(uint32_t mask, VkMemoryPropertyFlags wanted, uint32_t &type)
{
    AV_SCOPE("PresentDevice::memory_type");
    VkPhysicalDeviceMemoryProperties properties = {};
    call<MemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical_, &properties);
    /* The host's count is the host's: no more types are read than Vulkan has. */
    const uint32_t types =
        properties.memoryTypeCount < VK_MAX_MEMORY_TYPES ? properties.memoryTypeCount : VK_MAX_MEMORY_TYPES;
    for (uint32_t index = 0u; index < types; index++) {
        if ((mask & (1u << index)) == 0u) continue;
        if ((properties.memoryTypes[index].propertyFlags & wanted) != wanted) continue;
        type = index;
        return true;
    }
    return false;
}

VkResult PresentDevice::make_readback(VkDeviceSize bytes)
{
    AV_SCOPE("PresentDevice::make_readback");
    if (buffer_ != VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = bytes;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    const VkResult made = call<CreateBuffer>("vkCreateBuffer")(device_, &info, nullptr, &buffer_);
    if (made != VK_SUCCESS) {
        buffer_ = VK_NULL_HANDLE;
        return made;
    }
    VkMemoryRequirements needs = {};
    call<BufferRequirements>("vkGetBufferMemoryRequirements")(device_, buffer_, &needs);
    uint32_t type = 0u;
    if (!memory_type(needs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, type))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkMemoryAllocateInfo memory = {};
    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.allocationSize = needs.size;
    memory.memoryTypeIndex = type;
    VkResult step = call<Allocate>("vkAllocateMemory")(device_, &memory, nullptr, &buffer_memory_);
    if (step != VK_SUCCESS) {
        buffer_memory_ = VK_NULL_HANDLE;
        return step;
    }
    step = call<BindBuffer>("vkBindBufferMemory")(device_, buffer_, buffer_memory_, 0u);
    if (step != VK_SUCCESS) return step;
    void *view = nullptr;
    step = call<Map>("vkMapMemory")(device_, buffer_memory_, 0u, VK_WHOLE_SIZE, 0u, &view);
    if (step != VK_SUCCESS) return step;
    mapped_ = static_cast<uint8_t *>(view);
    return make_commands();
}

VkResult PresentDevice::make_commands()
{
    AV_SCOPE("PresentDevice::make_commands");
    if (pool_ != VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    VkCommandPoolCreateInfo pool = {};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = present_family;
    VkResult step = call<CreatePool>("vkCreateCommandPool")(device_, &pool, nullptr, &pool_);
    if (step != VK_SUCCESS) {
        pool_ = VK_NULL_HANDLE;
        return step;
    }
    VkCommandBufferAllocateInfo commands = {};
    commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commands.commandPool = pool_;
    commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commands.commandBufferCount = 1u;
    step = call<AllocateCommands>("vkAllocateCommandBuffers")(device_, &commands, &commands_);
    if (step != VK_SUCCESS) {
        commands_ = VK_NULL_HANDLE;
        return step;
    }
    VkFenceCreateInfo fence = {};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    step = call<CreateFence>("vkCreateFence")(device_, &fence, nullptr, &fence_);
    if (step != VK_SUCCESS) fence_ = VK_NULL_HANDLE;
    return step;
}

const uint8_t *PresentDevice::readback() const
{
    AV_SCOPE("PresentDevice::readback");
    return mapped_;
}

VkResult PresentDevice::copy(VkQueue queue, VkImage image, VkExtent2D extent, uint32_t wait_count,
                             const VkSemaphore *waits)
{
    AV_SCOPE("PresentDevice::copy");
    if (fence_ == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult recorded = record_copy(image, extent);
    if (recorded != VK_SUCCESS) return recorded;
    return run(queue, wait_count, waits);
}

VkResult PresentDevice::copy_into(VkQueue queue, VkImage image, VkImage target, VkExtent2D extent,
                                  uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("PresentDevice::copy_into");
    if (fence_ == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult recorded = record_copy_into(image, target, extent);
    if (recorded != VK_SUCCESS) return recorded;
    return run(queue, wait_count, waits);
}

/* The application's semaphores are the host's: the copy waits on them where
 * the host would have presented. */
VkResult PresentDevice::run(VkQueue queue, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("PresentDevice::run");
    if (wait_count > max_waits) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands_;
    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = waits;
    submit.pWaitDstStageMask = present_stages;
    VkResult step = call<Submit>("vkQueueSubmit")(queue, 1u, &submit, fence_);
    if (step != VK_SUCCESS) return step;
    step = call<WaitFences>("vkWaitForFences")(device_, 1u, &fence_, VK_TRUE, copy_timeout_ns);
    const VkResult reset = call<ResetFences>("vkResetFences")(device_, 1u, &fence_);
    if (step != VK_SUCCESS) return step == VK_TIMEOUT ? VK_ERROR_DEVICE_LOST : step;
    return reset;
}

VkResult PresentDevice::skip(VkQueue queue, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("PresentDevice::skip");
    if (wait_count == 0u) return VK_SUCCESS;
    if (wait_count > max_waits) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = waits;
    submit.pWaitDstStageMask = present_stages;
    return call<Submit>("vkQueueSubmit")(queue, 1u, &submit, VK_NULL_HANDLE);
}

VkResult PresentDevice::record_copy(VkImage image, VkExtent2D extent)
{
    AV_SCOPE("PresentDevice::record_copy");
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkResult begun = call<Begin>("vkBeginCommandBuffer")(commands_, &begin);
    if (begun != VK_SUCCESS) return begun;
    transition(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = {extent.width, extent.height, 1u};
    call<CopyToBuffer>("vkCmdCopyImageToBuffer")(commands_, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer_, 1u,
                                                 &region);
    transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return call<End>("vkEndCommandBuffer")(commands_);
}

/* The slot is overwritten whole, so it is taken from UNDEFINED: nothing it
 * held is kept, and no ownership needs taking back from the device that read
 * it last. A copy, not a blit: the slot has the image's channel order, and its
 * bytes cross unconverted. */
VkResult PresentDevice::record_copy_into(VkImage image, VkImage target, VkExtent2D extent)
{
    AV_SCOPE("PresentDevice::record_copy_into");
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkResult begun = call<Begin>("vkBeginCommandBuffer")(commands_, &begin);
    if (begun != VK_SUCCESS) return begun;
    transition(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy region = {};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1u;
    region.dstSubresource = region.srcSubresource;
    region.extent = {extent.width, extent.height, 1u};
    call<CopyImage>("vkCmdCopyImage")(commands_, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
    transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    release_to_external(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    return call<End>("vkEndCommandBuffer")(commands_);
}

void PresentDevice::transition(VkImage image, VkImageLayout from, VkImageLayout to)
{
    AV_SCOPE("PresentDevice::transition");
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.dstAccessMask = to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ? VK_ACCESS_TRANSFER_WRITE_BIT : 0u;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    call<Barrier>("vkCmdPipelineBarrier")(commands_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                                          0u, nullptr, 0u, nullptr, 1u, &barrier);
}

/* What the copy wrote made visible beyond this device, and the image handed to
 * the queue of whichever device reads it next, which is what has a driver
 * leave it in a state another driver can read. */
void PresentDevice::release_to_external(VkImage image, VkImageLayout from, VkImageLayout to)
{
    AV_SCOPE("PresentDevice::release_to_external");
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = present_family;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    call<Barrier>("vkCmdPipelineBarrier")(commands_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                                          &barrier);
}

void PresentDevice::release()
{
    AV_SCOPE("PresentDevice::release");
    if (fence_ != VK_NULL_HANDLE) call<DestroyFence>("vkDestroyFence")(device_, fence_, nullptr);
    /* A pool's command buffers go with it. */
    if (pool_ != VK_NULL_HANDLE) call<DestroyPool>("vkDestroyCommandPool")(device_, pool_, nullptr);
    if (mapped_ != nullptr) call<Unmap>("vkUnmapMemory")(device_, buffer_memory_);
    if (buffer_ != VK_NULL_HANDLE) call<DestroyBuffer>("vkDestroyBuffer")(device_, buffer_, nullptr);
    if (buffer_memory_ != VK_NULL_HANDLE) call<Free>("vkFreeMemory")(device_, buffer_memory_, nullptr);
    fence_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    commands_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
    buffer_ = VK_NULL_HANDLE;
    buffer_memory_ = VK_NULL_HANDLE;
    for (uint32_t index = image_count_; index > 0u; index--) {
        const PresentImage &made = made_[index - 1u];
        call<DestroyImage>("vkDestroyImage")(device_, made.image_, nullptr);
        call<Free>("vkFreeMemory")(device_, made.memory_, nullptr);
        made_[index - 1u] = PresentImage{};
    }
    image_count_ = 0u;
}

PresentImage PresentDevice::made(uint32_t index) const
{
    AV_SCOPE("PresentDevice::made");
    return index < image_count_ ? made_[index] : PresentImage{};
}

uint32_t PresentDevice::images() const
{
    AV_SCOPE("PresentDevice::images");
    return image_count_;
}

/* Every counted image has its memory, none past the count does, and a mapping
 * exists only over read-back memory. */
bool PresentDevice::invariants_hold() const
{
    AV_SCOPE("PresentDevice::invariants_hold");
    if (image_count_ > max_images) return false;
    for (uint32_t index = 0u; index < max_images; index++) {
        const bool counted = index < image_count_;
        if (counted != (made_[index].image_ != VK_NULL_HANDLE)) return false;
        if (counted != (made_[index].memory_ != VK_NULL_HANDLE)) return false;
    }
    return mapped_ == nullptr || buffer_memory_ != VK_NULL_HANDLE;
}

}  // namespace av::icd
