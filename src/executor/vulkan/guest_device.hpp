/* One device a guest's Vulkan context made, as the executor reaches it outside
 * the guest's own commands: its entry points (VulkanDeviceTable) and the
 * memory and semaphore imports and exports made through them
 * (VulkanMemoryImport). A shared object is exported through the device that
 * made it and imported into the device the guest names; the memory source's
 * blobs are imported onto the device the guest made last. */
#ifndef AV_GUEST_DEVICE_HPP
#define AV_GUEST_DEVICE_HPP

#include "executor/shim/vulkan_device_table.hpp"
#include "executor/shim/vulkan_memory_import.hpp"

#include <vulkan/vulkan_core.h>

namespace av {

class GuestDevice {
public:
    GuestDevice();
    GuestDevice(const GuestDevice &) = delete;
    GuestDevice &operator=(const GuestDevice &) = delete;

    /* `device`, made on `physical` of `instance`, with every entry point it
     * answers; false for a null handle. */
    [[nodiscard]] bool  load(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    VkDevice            device() const;
    VulkanDeviceTable  &table();
    VulkanMemoryImport &memory();

private:
    VulkanDeviceTable  table_;
    /* Over table_, so declared after it. */
    VulkanMemoryImport memory_;
};

}  // namespace av

#endif  // AV_GUEST_DEVICE_HPP
