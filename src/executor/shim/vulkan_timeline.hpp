#ifndef AV_VULKAN_TIMELINE_HPP
#define AV_VULKAN_TIMELINE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanTimeline {
public:
    VulkanTimeline();
    ~VulkanTimeline();
    VulkanTimeline(const VulkanTimeline &) = delete;
    VulkanTimeline &operator=(const VulkanTimeline &) = delete;

    [[nodiscard]] bool make_local(const VulkanDeviceTable &table);
    [[nodiscard]] bool make_exportable(const VulkanDeviceTable &table);
    [[nodiscard]] bool import_fd(const VulkanDeviceTable &table, OwnedFd &descriptor);
    [[nodiscard]] bool export_fd(OwnedFd &descriptor) const;
    [[nodiscard]] bool wait(uint64_t value, uint64_t nanoseconds) const;
    void               release();

    bool        made() const;
    VkSemaphore semaphore() const;

private:
    const VulkanDeviceTable *table_;
    VkSemaphore              semaphore_;
};

}

#endif
