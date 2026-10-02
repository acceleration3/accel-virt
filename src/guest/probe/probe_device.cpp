#include "guest/probe/probe_device.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/vulkan_entry.hpp"

namespace av {

namespace {

/* VK_API_VERSION_1_3, spelled without the header's macro and its C casts. */
constexpr uint32_t   api_version_1_3 = (1u << 22u) | (3u << 12u);
constexpr float      queue_priority = 1.0f;
constexpr VkQueueFlags draws_and_computes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

}  // namespace

ProbeDevice::ProbeDevice(VulkanEntry &vulkan)
    : vulkan_(vulkan), instance_(VK_NULL_HANDLE), listed_{}, listed_count_(0u), physical_device_(VK_NULL_HANDLE),
      device_(VK_NULL_HANDLE), queue_(VK_NULL_HANDLE), family_(0u), memory_{}
      {
          AV_SCOPE("ProbeDevice::ProbeDevice");
      }

ProbeDevice::~ProbeDevice()
{
    AV_SCOPE("ProbeDevice::~ProbeDevice");
    if (device_ != VK_NULL_HANDLE) vulkan_.destroy_device(device_);
    if (instance_ != VK_NULL_HANDLE) vulkan_.destroy_instance(instance_);
}

/* What a windowed application enables: a surface on a Win32 window, and the
 * swapchain that presents on it. */
const char *const ProbeDevice::instance_extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
const char *const ProbeDevice::device_extensions[] = {"VK_KHR_swapchain"};

VkResult ProbeDevice::make_instance()
{
    AV_SCOPE("ProbeDevice::make_instance");
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "avvkprobe";
    application.apiVersion = api_version_1_3;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &application;
    /* The window-system extensions the present steps use. An application that
     * calls vkCreateWin32SurfaceKHR without enabling them reaches a dispatch
     * slot the Khronos loader never filled, which is a jump to address zero -
     * how the live probe of feature 046 first crashed. */
    info.enabledExtensionCount = instance_extension_count;
    info.ppEnabledExtensionNames = instance_extensions;
    return vulkan_.create_instance(&info, &instance_);
}

VkResult ProbeDevice::list_physical_devices()
{
    AV_SCOPE("ProbeDevice::list_physical_devices");
    uint32_t       room = max_physical_devices;
    const VkResult listed = vulkan_.enumerate_physical_devices(instance_, &room, listed_);
    if (listed != VK_SUCCESS && listed != VK_INCOMPLETE) return listed;
    listed_count_ = room;
    return VK_SUCCESS;
}

bool ProbeDevice::choose(uint64_t luid)
{
    AV_SCOPE("ProbeDevice::choose");
    for (uint32_t index = 0u; index < listed_count_; index++) {
        VkPhysicalDeviceIDProperties id = {};
        id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &id;
        vulkan_.physical_device_properties2(listed_[index], &properties);
        uint64_t named = 0u;
        __builtin_memcpy(&named, id.deviceLUID, sizeof named);
        if (id.deviceLUIDValid != VK_TRUE || named != luid) continue;
        physical_device_ = listed_[index];
        return true;
    }
    return false;
}

bool ProbeDevice::choose_family()
{
    AV_SCOPE("ProbeDevice::choose_family");
    uint32_t                count = max_queue_families;
    VkQueueFamilyProperties families[max_queue_families] = {};
    vulkan_.queue_family_properties(physical_device_, &count, families);
    for (uint32_t index = 0u; index < count; index++) {
        if ((families[index].queueFlags & draws_and_computes) != draws_and_computes) continue;
        family_ = index;
        return true;
    }
    return false;
}

VkResult ProbeDevice::make_device(const char *const *extra, uint32_t extra_count)
{
    AV_SCOPE("ProbeDevice::make_device");
    if (extra_count > max_device_extensions - device_extension_count) return VK_ERROR_EXTENSION_NOT_PRESENT;
    const char *asked[max_device_extensions] = {};
    for (uint32_t index = 0u; index < device_extension_count; index++) asked[index] = device_extensions[index];
    for (uint32_t index = 0u; index < extra_count; index++)
        asked[device_extension_count + index] = extra[index];
    vulkan_.memory_properties(physical_device_, &memory_);
    VkDeviceQueueCreateInfo queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = family_;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &queue_priority;
    VkDeviceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1u;
    info.pQueueCreateInfos = &queue;
    info.enabledExtensionCount = device_extension_count + extra_count;
    info.ppEnabledExtensionNames = asked;
    const VkResult made = vulkan_.create_device(physical_device_, &info, &device_);
    if (made != VK_SUCCESS) return made;
    vulkan_.device_queue(device_, family_, 0u, &queue_);
    return VK_SUCCESS;
}

bool ProbeDevice::memory_type(uint32_t allowed, VkMemoryPropertyFlags wanted, uint32_t &type) const
{
    AV_SCOPE("ProbeDevice::memory_type");
    for (uint32_t index = 0u; index < memory_.memoryTypeCount && index < VK_MAX_MEMORY_TYPES; index++) {
        const bool usable = (allowed >> index & 1u) != 0u;
        if (!usable || (memory_.memoryTypes[index].propertyFlags & wanted) != wanted) continue;
        type = index;
        return true;
    }
    return false;
}

VulkanEntry     &ProbeDevice::vulkan() const
{
    AV_SCOPE("ProbeDevice::vulkan");
    return vulkan_;
}
VkInstance       ProbeDevice::instance() const
{
    AV_SCOPE("ProbeDevice::instance");
    return instance_;
}
VkPhysicalDevice ProbeDevice::physical_device() const
{
    AV_SCOPE("ProbeDevice::physical_device");
    return physical_device_;
}
VkDevice         ProbeDevice::device() const
{
    AV_SCOPE("ProbeDevice::device");
    return device_;
}
VkQueue          ProbeDevice::queue() const
{
    AV_SCOPE("ProbeDevice::queue");
    return queue_;
}
uint32_t         ProbeDevice::family() const
{
    AV_SCOPE("ProbeDevice::family");
    return family_;
}
uint32_t         ProbeDevice::listed() const
{
    AV_SCOPE("ProbeDevice::listed");
    return listed_count_;
}

bool ProbeDevice::invariants_hold() const
{
    AV_SCOPE("ProbeDevice::invariants_hold");
    if (listed_count_ > max_physical_devices || family_ >= max_queue_families) return false;
    if (listed_count_ != 0u && instance_ == VK_NULL_HANDLE) return false;
    if (physical_device_ != VK_NULL_HANDLE && listed_count_ == 0u) return false;
    if (device_ != VK_NULL_HANDLE && physical_device_ == VK_NULL_HANDLE) return false;
    return (queue_ == VK_NULL_HANDLE) == (device_ == VK_NULL_HANDLE);
}

}  // namespace av
