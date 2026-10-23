/* What the executor adds to a queue an API's runtime lends it (BorrowedQueue):
 * an image copy that signals a timeline once it is done, and a wait that holds
 * everything the runtime submits after it until a timeline reaches a value.
 * One per device that copies or waits - the compositor's images reach each API
 * through these, with nothing on the CPU waiting for the GPU. */
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
    /* Copies in flight at once; past this the oldest is waited for, which a
     * reader that keeps up never makes happen. */
    static constexpr uint32_t in_flight = 4u;

    QueueWork();
    ~QueueWork();
    QueueWork(const QueueWork &) = delete;
    QueueWork &operator=(const QueueWork &) = delete;

    /* Work on `queue` of `table`'s device; both must outlive this. */
    [[nodiscard]] bool open(const VulkanDeviceTable &table, BorrowedQueue &queue);
    bool               opened() const;
    /* What was submitted is done, and the command buffers are let go of. */
    void               close();

    /* `source`, in `layout`, into `destination`, `width` by `height`, behind
     * everything the runtime recorded; `signal` reaches `value` once it is
     * done. The source is left in `layout`, the destination GENERAL. */
    [[nodiscard]] bool copy(VkImage source, VkImageLayout layout, VkImage destination, uint32_t width,
                            uint32_t height, const VulkanTimeline &signal, uint64_t value);
    /* Everything the runtime submits after this waits for `timeline` to reach
     * `value`. */
    [[nodiscard]] bool wait(const VulkanTimeline &timeline, uint64_t value);
    /* Signals after everything the runtime has recorded. */
    [[nodiscard]] bool signal(const VulkanTimeline &timeline, uint64_t value);
    /* Everything submitted to the queue is done. */
    void               idle();

private:
    /* A command buffer whose last copy is done, waiting for it if not. */
    VkCommandBuffer next_buffer();
    void            record(VkCommandBuffer buffer, VkImage source, VkImageLayout layout, VkImage destination,
                           uint32_t width, uint32_t height) const;
    /* `buffer`, recorded, submitted to signal `value` on `signal`; kept in
     * `slot` until that is reached. */
    [[nodiscard]] bool send(uint32_t slot, VkCommandBuffer buffer, const VulkanTimeline &signal, uint64_t value);

    const VulkanDeviceTable *table_;
    BorrowedQueue           *queue_;
    VkCommandPool            pool_;
    VkCommandBuffer          buffers_[in_flight];
    /* What each buffer's copy signals, and on which timeline. */
    const VulkanTimeline    *signals_[in_flight];
    uint64_t                 values_[in_flight];
    uint32_t                 next_;
};

}  // namespace av

#endif  // AV_QUEUE_WORK_HPP
