#include "guest/probe/probe_commands.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/probe/vulkan_entry.hpp"

namespace av {

ProbeCommands::ProbeCommands(ProbeDevice &device)
    : device_(device), pool_(VK_NULL_HANDLE), buffer_(VK_NULL_HANDLE), fence_(VK_NULL_HANDLE)
    {
        AV_SCOPE("ProbeCommands::ProbeCommands");
    }

/* Vulkan makes destroying VK_NULL_HANDLE a no-op; the buffer is the pool's. */
ProbeCommands::~ProbeCommands()
{
    AV_SCOPE("ProbeCommands::~ProbeCommands");
    device_.vulkan().destroy_fence(device_.device(), fence_);
    device_.vulkan().destroy_command_pool(device_.device(), pool_);
}

bool ProbeCommands::begin(ProbeOutcome &outcome)
{
    AV_SCOPE("ProbeCommands::begin");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    VkCommandPoolCreateInfo pool = {};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.queueFamilyIndex = device_.family();
    if (!outcome.accept(vulkan.create_command_pool(device, &pool, &pool_))) return false;
    VkCommandBufferAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = pool_;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1u;
    if (!outcome.accept(vulkan.allocate_command_buffer(device, &allocation, &buffer_))) return false;
    VkFenceCreateInfo fence = {};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!outcome.accept(vulkan.create_fence(device, &fence, &fence_))) return false;
    VkCommandBufferBeginInfo recording = {};
    recording.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    recording.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return outcome.accept(vulkan.begin_command_buffer(buffer_, &recording));
}

bool ProbeCommands::run(ProbeOutcome &outcome)
{
    AV_SCOPE("ProbeCommands::run");
    VulkanEntry &vulkan = device_.vulkan();
    if (!outcome.accept(vulkan.end_command_buffer(buffer_))) return false;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &buffer_;
    if (!outcome.accept(vulkan.queue_submit(device_.queue(), &submit, fence_))) return false;
    return outcome.accept(vulkan.wait_for_fence(device_.device(), fence_, wait_nanoseconds));
}

VkCommandBuffer ProbeCommands::buffer() const
{
    AV_SCOPE("ProbeCommands::buffer");
    return buffer_;
}

bool ProbeCommands::invariants_hold() const
{
    AV_SCOPE("ProbeCommands::invariants_hold");
    return pool_ != VK_NULL_HANDLE || (buffer_ == VK_NULL_HANDLE && fence_ == VK_NULL_HANDLE);
}

}  // namespace av
