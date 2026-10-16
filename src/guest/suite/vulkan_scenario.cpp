#include "guest/suite/vulkan_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av {

VulkanScenario::VulkanScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "vulkan"), link_(kmt, search, steps_, SubApi::vulkan), chain_(steps_, link_.transport()),
      buffer_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE)
      {
          AV_SCOPE("VulkanScenario::VulkanScenario");
      }

void VulkanScenario::run()
{
    AV_SCOPE("VulkanScenario::run");
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    Status built = chain_.make();
    if (built == Status::ok) built = bind_buffer();
    const Status destroyed = destroy();
    if (built == Status::ok) static_cast<void>(steps_.check("destroy", destroyed));
}

Status VulkanScenario::bind_buffer()
{
    AV_SCOPE("VulkanScenario::bind_buffer");
    KmtTransport      &transport = link_.transport();
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_bytes;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer       buffer = VK_NULL_HANDLE;
    const VkResult created = vkproto::av_vkCreateBuffer(transport, chain_.handles(), chain_.device(), &buffer_info, nullptr, &buffer);
    const Status   buffered = steps_.check_result("buffer", created);
    if (buffered != Status::ok) return buffered;
    buffer_ = buffer;
    const Status allocated = allocate_memory();
    if (allocated != Status::ok) return allocated;
    const VkResult bound = vkproto::av_vkBindBufferMemory(transport, chain_.handles(), chain_.device(), buffer_, memory_, 0u);
    return steps_.check_result("bind", bound);
}

Status VulkanScenario::allocate_memory()
{
    AV_SCOPE("VulkanScenario::allocate_memory");
    KmtTransport        &transport = link_.transport();
    const VkDevice       device = chain_.device();
    VkMemoryRequirements requirements = {};
    vkproto::av_vkGetBufferMemoryRequirements(transport, chain_.handles(), device, buffer_, &requirements);
    /* The call answers no result, and a buffer always has somewhere to live:
     * no memory type at all means the answer never came. */
    if (requirements.memoryTypeBits == 0u) return steps_.check_measured("memory", Status::not_found, 0u);
    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = static_cast<uint32_t>(__builtin_ctz(requirements.memoryTypeBits));
    VkDeviceMemory memory = VK_NULL_HANDLE;
    const Status   allocated =
        steps_.check_result("memory", vkproto::av_vkAllocateMemory(transport, chain_.handles(), device, &allocate, nullptr, &memory));
    if (allocated == Status::ok) memory_ = memory;
    return allocated;
}

/* A null buffer or memory is the API's own no-op; the device and instance the
 * chain names only if it made them. */
Status VulkanScenario::destroy()
{
    AV_SCOPE("VulkanScenario::destroy");
    KmtTransport  &transport = link_.transport();
    const VkDevice device = chain_.device();
    if (device != VK_NULL_HANDLE) {
        vkproto::av_vkFreeMemory(transport, chain_.handles(), device, memory_, nullptr);
        vkproto::av_vkDestroyBuffer(transport, chain_.handles(), device, buffer_, nullptr);
    }
    chain_.destroy();
    memory_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    uint32_t answered = 0u;
    return transport.nop(answered);
}

}  // namespace av
