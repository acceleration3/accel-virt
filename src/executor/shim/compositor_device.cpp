#include "executor/shim/compositor_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/physical_device_identity.hpp"

#include <vulkan/vulkan.h>

namespace av {

namespace {

/* The modifier extension because the images shared from here are laid out by
 * one (ExportableImageInfo). */
const char *const device_extensions[] = {VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                                          VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
                                          VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
                                          VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME};
constexpr uint32_t device_extension_count = sizeof device_extensions / sizeof device_extensions[0];
constexpr uint32_t most_physical_devices = 16u;
/* VK_API_VERSION_1_2, whose macro casts as C does: timeline semaphores are
 * core from it. */
constexpr uint32_t api_version_1_2 = (1u << 22u) | (2u << 12u);

}  // namespace

CompositorDevice::CompositorDevice() : instance_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE)
{
    AV_SCOPE("CompositorDevice::CompositorDevice");
}

CompositorDevice::~CompositorDevice()
{
    AV_SCOPE("CompositorDevice::~CompositorDevice");
    close();
}

bool CompositorDevice::open(const uint8_t *uuid)
{
    AV_SCOPE("CompositorDevice::open");
    if (device_ != VK_NULL_HANDLE) return true;
    if (uuid == nullptr) return false;
    const VkApplicationInfo    app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "accel-virt compositor", 1u,
                                      "accel-virt", 1u, api_version_1_2};
    const VkInstanceCreateInfo instance = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0u, &app, 0u, nullptr,
                                           0u, nullptr};
    if (vkCreateInstance(&instance, nullptr, &instance_) != VK_SUCCESS) {
        instance_ = VK_NULL_HANDLE;
        return false;
    }
    VkPhysicalDevice physicals[most_physical_devices];
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t         count = most_physical_devices;
    if (vkEnumeratePhysicalDevices(instance_, &count, physicals) < VK_SUCCESS) count = 0u;
    for (uint32_t index = 0u; index < count && physical == VK_NULL_HANDLE; index++)
        if (PhysicalDeviceIdentity(instance_, physicals[index]).is(uuid)) physical = physicals[index];
    const float                      priority = 1.0f;
    const VkDeviceQueueCreateInfo    queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0u, 0u, 1u,
                                              &priority};
    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.timelineSemaphore = VK_TRUE;
    const VkDeviceCreateInfo created = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &features12, 0u, 1u, &queue, 0u,
                                        nullptr, device_extension_count, device_extensions, nullptr};
    if (physical == VK_NULL_HANDLE || vkCreateDevice(physical, &created, nullptr, &device_) != VK_SUCCESS) {
        device_ = VK_NULL_HANDLE;
        close();
        return false;
    }
    if (table_.load(instance_, physical, device_)) return true;
    close();
    return false;
}

bool CompositorDevice::opened() const
{
    AV_SCOPE("CompositorDevice::opened");
    return table_.loaded();
}

void CompositorDevice::close()
{
    AV_SCOPE("CompositorDevice::close");
    table_.forget();
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    device_ = VK_NULL_HANDLE;
    instance_ = VK_NULL_HANDLE;
}

const VulkanDeviceTable &CompositorDevice::table() const
{
    AV_SCOPE("CompositorDevice::table");
    return table_;
}

}  // namespace av
