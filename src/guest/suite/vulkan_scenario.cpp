#include "guest/suite/vulkan_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av {

namespace {

/* VK_API_VERSION_1_1, spelled without the header's macro and its C casts. */
constexpr uint32_t api_version_1_1 = (1u << 22u) | (1u << 12u);
constexpr float    queue_priority = 1.0f;
/* Every Vulkan device has a family 0, and one queue of it is all a buffer's
 * binding needs. */
constexpr uint32_t queue_family = 0u;

}  // namespace

VulkanScenario::VulkanScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "vulkan"), link_(kmt, search, steps_, SubApi::vulkan), instance_(0u),
      physical_device_(0u), device_(0u), buffer_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE) {}

void VulkanScenario::run()
{
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    Status built = make_device();
    if (built == Status::ok) built = bind_buffer();
    const Status destroyed = destroy();
    if (built == Status::ok) static_cast<void>(steps_.check("destroy", destroyed));
}

Status VulkanScenario::make_device()
{
    KmtTransport     &transport = link_.transport();
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "rvktest";
    application.apiVersion = api_version_1_1;
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    uint64_t       instance = 0u;
    const VkResult made = vkproto::rvk_vkCreateInstance(transport, &instance_info, nullptr, &instance);
    const Status   instanced = steps_.check_result("instance", made);
    if (instanced != Status::ok) return instanced;
    instance_ = instance;
    const Status found = find_physical_device();
    if (found != Status::ok) return found;

    float                   priority = queue_priority;
    VkDeviceQueueCreateInfo queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = queue_family;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1u;
    device_info.pQueueCreateInfos = &queue;
    uint64_t       device = 0u;
    const VkResult created = vkproto::rvk_vkCreateDevice(
        transport, reinterpret_cast<VkPhysicalDevice>(physical_device_), &device_info, nullptr, &device);
    const Status judged = steps_.check_result("device", created);
    if (judged == Status::ok) device_ = device;
    return judged;
}

/* Asked twice, the way an application does: how many, then as many as there
 * is room for. The count is what the step reports, and a host with none fails
 * it; one with more than the room answers VK_INCOMPLETE, which fails it too. */
Status VulkanScenario::find_physical_device()
{
    KmtTransport    &transport = link_.transport();
    const VkInstance instance = reinterpret_cast<VkInstance>(instance_);
    uint32_t         count = 0u;
    VkResult         listed = vkproto::rvk_vkEnumeratePhysicalDevices(transport, instance, &count, nullptr);
    uint64_t         devices[max_physical_devices] = {};
    uint32_t         room = max_physical_devices;
    if (listed == VK_SUCCESS && count != 0u)
        listed = vkproto::rvk_vkEnumeratePhysicalDevices(transport, instance, &room, devices);
    if (listed != VK_SUCCESS) return steps_.check_result("physical_device", listed);
    const Status found = steps_.check_measured("physical_device", count != 0u ? Status::ok : Status::not_found, count);
    if (found == Status::ok) physical_device_ = devices[0];
    return found;
}

Status VulkanScenario::bind_buffer()
{
    KmtTransport      &transport = link_.transport();
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_bytes;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer       buffer = VK_NULL_HANDLE;
    const VkResult created =
        vkproto::rvk_vkCreateBuffer(transport, reinterpret_cast<VkDevice>(device_), &buffer_info, nullptr, &buffer);
    const Status buffered = steps_.check_result("buffer", created);
    if (buffered != Status::ok) return buffered;
    buffer_ = buffer;
    const Status allocated = allocate_memory();
    if (allocated != Status::ok) return allocated;
    const VkResult bound =
        vkproto::rvk_vkBindBufferMemory(transport, reinterpret_cast<VkDevice>(device_), buffer_, memory_, 0u);
    return steps_.check_result("bind", bound);
}

Status VulkanScenario::allocate_memory()
{
    KmtTransport        &transport = link_.transport();
    const VkDevice       device = reinterpret_cast<VkDevice>(device_);
    VkMemoryRequirements requirements = {};
    vkproto::rvk_vkGetBufferMemoryRequirements(transport, device, buffer_, &requirements);
    /* The call answers no result, and a buffer always has somewhere to live:
     * no memory type at all means the answer never came. */
    if (requirements.memoryTypeBits == 0u) return steps_.check_measured("memory", Status::not_found, 0u);
    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = static_cast<uint32_t>(__builtin_ctz(requirements.memoryTypeBits));
    VkDeviceMemory memory = VK_NULL_HANDLE;
    const Status   allocated =
        steps_.check_result("memory", vkproto::rvk_vkAllocateMemory(transport, device, &allocate, nullptr, &memory));
    if (allocated == Status::ok) memory_ = memory;
    return allocated;
}

/* A null buffer or memory is the API's own no-op, but a null device or
 * instance is refused by the host as undecodable and would fault the context. */
Status VulkanScenario::destroy()
{
    KmtTransport  &transport = link_.transport();
    const VkDevice device = reinterpret_cast<VkDevice>(device_);
    if (device_ != 0u) {
        vkproto::rvk_vkFreeMemory(transport, device, memory_, nullptr);
        vkproto::rvk_vkDestroyBuffer(transport, device, buffer_, nullptr);
        vkproto::rvk_vkDestroyDevice(transport, device, nullptr);
    }
    if (instance_ != 0u) vkproto::rvk_vkDestroyInstance(transport, reinterpret_cast<VkInstance>(instance_), nullptr);
    memory_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    device_ = 0u;
    physical_device_ = 0u;
    instance_ = 0u;
    uint32_t answered = 0u;
    return transport.nop(answered);
}

}  // namespace av
