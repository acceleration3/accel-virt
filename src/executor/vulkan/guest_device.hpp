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

    [[nodiscard]] bool  load(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    VkDevice            device() const;
    VulkanDeviceTable  &table();
    VulkanMemoryImport &memory();

private:
    VulkanDeviceTable  table_;
    VulkanMemoryImport memory_;
};

}

#endif
