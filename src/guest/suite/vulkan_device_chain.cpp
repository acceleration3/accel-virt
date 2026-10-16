#include "guest/suite/vulkan_device_chain.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av {

namespace {

/* VK_API_VERSION_1_1, spelled without the header's macro and its C casts. */
constexpr uint32_t api_version_1_1 = (1u << 22u) | (1u << 12u);
constexpr float    queue_priority = 1.0f;
/* Every Vulkan device has a family 0, and one queue of it is all a scenario
 * submits to. */
constexpr uint32_t queue_family = 0u;

}  // namespace

VulkanDeviceChain::VulkanDeviceChain(ScenarioSteps &steps, ClientTransport &transport)
    : steps_(steps), transport_(transport), handles_(), instance_(0u), physical_device_(0u), device_(0u)
    {
        AV_SCOPE("VulkanDeviceChain::VulkanDeviceChain");
    }

Status VulkanDeviceChain::make()
{
    AV_SCOPE("VulkanDeviceChain::make");
    const Status instanced = make_instance();
    if (instanced != Status::ok) return instanced;
    const Status found = find_physical_device();
    if (found != Status::ok) return found;
    return make_device();
}

Status VulkanDeviceChain::make_instance()
{
    AV_SCOPE("VulkanDeviceChain::make_instance");
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "avtest";
    application.apiVersion = api_version_1_1;
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    uint64_t       instance = 0u;
    const VkResult made = vkproto::av_vkCreateInstance(transport_, handles_, &instance_info, nullptr, &instance);
    const Status   instanced = steps_.check_result("instance", made);
    if (instanced == Status::ok) instance_ = instance;
    return instanced;
}

/* Asked twice, the way an application does: how many, then as many as there
 * is room for. The count is what the step reports, and a host with none fails
 * it; one with more than the room answers VK_INCOMPLETE, which fails it too. */
Status VulkanDeviceChain::find_physical_device()
{
    AV_SCOPE("VulkanDeviceChain::find_physical_device");
    const VkInstance instance = reinterpret_cast<VkInstance>(instance_);
    uint32_t         count = 0u;
    VkResult         listed = vkproto::av_vkEnumeratePhysicalDevices(transport_, handles_, instance, &count, nullptr);
    uint64_t         devices[max_physical_devices] = {};
    uint32_t         room = max_physical_devices;
    if (listed == VK_SUCCESS && count != 0u)
        listed = vkproto::av_vkEnumeratePhysicalDevices(transport_, handles_, instance, &room, devices);
    if (listed != VK_SUCCESS) return steps_.check_result("physical_device", listed);
    const Status found = steps_.check_measured("physical_device", count != 0u ? Status::ok : Status::not_found, count);
    if (found == Status::ok) physical_device_ = devices[0];
    return found;
}

Status VulkanDeviceChain::make_device()
{
    AV_SCOPE("VulkanDeviceChain::make_device");
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
    const VkResult created =
        vkproto::av_vkCreateDevice(transport_, handles_, physical_device(), &device_info, nullptr, &device);
    const Status judged = steps_.check_result("device", created);
    if (judged == Status::ok) device_ = device;
    return judged;
}

/* A null device or instance is refused by the host as undecodable and would
 * fault the context, so only what was made is named. */
void VulkanDeviceChain::destroy()
{
    AV_SCOPE("VulkanDeviceChain::destroy");
    if (device_ != 0u) vkproto::av_vkDestroyDevice(transport_, handles_, device(), nullptr);
    if (instance_ != 0u) vkproto::av_vkDestroyInstance(transport_, handles_, reinterpret_cast<VkInstance>(instance_), nullptr);
    device_ = 0u;
    physical_device_ = 0u;
    instance_ = 0u;
}

VkPhysicalDevice VulkanDeviceChain::physical_device() const
{
    AV_SCOPE("VulkanDeviceChain::physical_device");
    return reinterpret_cast<VkPhysicalDevice>(physical_device_);
}

VkDevice VulkanDeviceChain::device() const
{
    AV_SCOPE("VulkanDeviceChain::device");
    return reinterpret_cast<VkDevice>(device_);
}

vkproto::HandleTranslation &VulkanDeviceChain::handles()
{
    AV_SCOPE("VulkanDeviceChain::handles");
    return handles_;
}

/* Each handle is only ever held with the one it was made from. */
bool VulkanDeviceChain::invariants_hold() const
{
    AV_SCOPE("VulkanDeviceChain::invariants_hold");
    return (device_ == 0u || physical_device_ != 0u) && (physical_device_ == 0u || instance_ != 0u);
}

}  // namespace av
