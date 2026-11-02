#ifndef AV_FRAME_BUFFER_COPY_HPP
#define AV_FRAME_BUFFER_COPY_HPP

#include "device/abi/av_executor.h"
#include "device/shim/posix_lock.hpp"
#include "executor/shim/queue_work.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class BorrowedQueue;
class VulkanDeviceTable;
struct FrameBufferCopyProbe;

class FrameBufferCopy {
public:
    static constexpr uint32_t most_imports = 4u;

    FrameBufferCopy();
    ~FrameBufferCopy();
    FrameBufferCopy(const FrameBufferCopy &) = delete;
    FrameBufferCopy &operator=(const FrameBufferCopy &) = delete;

    [[nodiscard]] bool open(const VulkanDeviceTable &table, BorrowedQueue &queue);
    bool               opened() const;
    void               close();

    [[nodiscard]] bool submit(VkImage image, VkImageLayout layout, const VkImageCreateInfo &described,
                              const AvFrameCopy &target, uint64_t &value);
    [[nodiscard]] bool wait(uint64_t value, uint32_t wait_ms) const;
    uint32_t           epoch() const;
    [[nodiscard]] bool finished(uint32_t epoch, uint64_t value, uint32_t wait_ms) const;

private:
    struct Import {
        uint64_t     lending_;
        VulkanBuffer buffer_;
        uint64_t     value_;
        uint64_t     used_;
    };

    Import *import_for(const AvFrameCopy &target);
    Import *room(uint64_t oldest_live);
    bool    place(const VkImageCreateInfo &described, const AvFrameCopy &target, VkBufferImageCopy *placed) const;
    bool    overlap(const AvFrameRegion &first, const AvFrameRegion &second) const;
    [[nodiscard]] bool forget(Import &import);
    void    drop_dead(uint64_t oldest_live);
    void    release(Import &import);

    const VulkanDeviceTable *table_;
    PosixLock                lock_;
    VulkanTimeline           timeline_;
    QueueWork                work_;
    Import                   imports_[most_imports];
    uint64_t                 value_;
    uint64_t                 clock_;
    mutable uint32_t         waiters_;
    uint32_t                 closing_;
    uint32_t                 epoch_;

    friend struct FrameBufferCopyProbe;
};

}

#endif
