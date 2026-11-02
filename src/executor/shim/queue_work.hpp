#ifndef AV_QUEUE_WORK_HPP
#define AV_QUEUE_WORK_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class BorrowedQueue;
class VulkanDeviceTable;
class VulkanTimeline;

class QueueWork {
public:
    static constexpr uint32_t in_flight = 4u;

    QueueWork();
    ~QueueWork();
    QueueWork(const QueueWork &) = delete;
    QueueWork &operator=(const QueueWork &) = delete;

    [[nodiscard]] bool open(const VulkanDeviceTable &table, BorrowedQueue &queue);
    bool               opened() const;
    void               close();

    [[nodiscard]] bool copy(VkImage source, VkImageLayout layout, VkImage destination, uint32_t width,
                            uint32_t height, const VulkanTimeline &signal, uint64_t value);
    [[nodiscard]] bool copy_to_buffer(VkImage source, VkImageLayout layout, VkBuffer destination,
                                      const VkBufferImageCopy *regions, uint32_t count, const VulkanTimeline &signal,
                                      uint64_t value);
    [[nodiscard]] bool wait(const VulkanTimeline &timeline, uint64_t value);
    [[nodiscard]] bool signal(const VulkanTimeline &timeline, uint64_t value);
    void               idle();

private:
    VkCommandBuffer next_buffer();
    void            record(VkCommandBuffer buffer, VkImage source, VkImageLayout layout, VkImage destination,
                           uint32_t width, uint32_t height) const;
    void            record_to_buffer(VkCommandBuffer buffer, VkImage source, VkImageLayout layout,
                                     VkBuffer destination, const VkBufferImageCopy *regions, uint32_t count) const;
    [[nodiscard]] bool send(uint32_t slot, VkCommandBuffer buffer, const VulkanTimeline &signal, uint64_t value);

    const VulkanDeviceTable *table_;
    BorrowedQueue           *queue_;
    VkCommandPool            pool_;
    VkCommandBuffer          buffers_[in_flight];
    const VulkanTimeline    *signals_[in_flight];
    uint64_t                 values_[in_flight];
    uint32_t                 next_;
};

}

#endif
