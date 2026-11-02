/* A frame's image copied by the GPU into guest pages lent to it
 * (docs/compositor.md): the lending's dma-buf imported as a buffer on the
 * device the frame is on, and `vkCmdCopyImageToBuffer` run on the queue the
 * frame's API owns (QueueWork), behind everything that API recorded - so the
 * frame is complete - with a timeline for the caller to wait on. Shared by
 * every API whose frames are Vulkan images (Direct3D 9 and 11 through DXVK).
 *
 * A buffer is imported once per lending and kept, by the lending's id, which
 * is never given to other memory: a lending given back is never hit again,
 * and its buffer is let go when the device says its id is older than every
 * live lending, or else is the oldest to go once the cache is full. A buffer
 * is let go only after the copies that wrote it are done - never before.
 *
 * Any thread: the present engine's. submit and wait are separate so a caller
 * holds its API's locks only across the submit; this object's own lock is
 * held across that alone, never across the wait. */
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
    /* Lendings kept imported at once. */
    static constexpr uint32_t most_imports = 4u;

    FrameBufferCopy();
    ~FrameBufferCopy();
    FrameBufferCopy(const FrameBufferCopy &) = delete;
    FrameBufferCopy &operator=(const FrameBufferCopy &) = delete;

    /* Work on `queue` of `table`'s device; both must outlive this. */
    [[nodiscard]] bool open(const VulkanDeviceTable &table, BorrowedQueue &queue);
    bool               opened() const;
    /* Waiters are waited out, the copies in flight are done and every import
     * let go. */
    void               close();

    /* `image`, as `described` says DXVK made it, in `layout`, submitted to be
     * copied as `target` asks, behind what the API recorded; the copy's
     * timeline value in `value`. Under the API's locks: it flushes what the
     * API recorded and takes the queue's. False, nothing submitted, where the
     * copy cannot be made - an image that is not single-sampled, readable by
     * a transfer and four bytes a texel, a region outside the image or the
     * memory, memory that cannot be imported here or is not coherent for the
     * CPU. No region is a copy already done: `value` 0, which
     * wait finds reached. */
    [[nodiscard]] bool submit(VkImage image, VkImageLayout layout, const VkImageCreateInfo &described,
                              const AvFrameCopy &target, uint64_t &value);
    /* The copy `value` is done and its bytes visible, within `wait_ms`. Takes
     * no lock. */
    [[nodiscard]] bool wait(uint64_t value, uint32_t wait_ms) const;
    /* Which opening the timeline values are of: they start again at each. */
    uint32_t           epoch() const;
    /* The copy `value` of opening `epoch` is done: true at once for one of an
     * opening that was closed, which left the queue idle. */
    [[nodiscard]] bool finished(uint32_t epoch, uint64_t value, uint32_t wait_ms) const;

private:
    struct Import {
        uint64_t     lending_;
        VulkanBuffer buffer_;
        /* The last copy submitted into it, and when. */
        uint64_t     value_;
        uint64_t     used_;
    };

    /* Under the lock: the buffer over `target`'s lending, imported if it is
     * not; null where it cannot be. */
    Import *import_for(const AvFrameCopy &target);
    /* Under the lock: a slot to import into - a dead lending's, else the least
     * recently used one emptied; null when what is there is still being
     * copied into. */
    Import *room(uint64_t oldest_live);
    bool    place(const VkImageCreateInfo &described, const AvFrameCopy &target, VkBufferImageCopy *placed) const;
    bool    overlap(const AvFrameRegion &first, const AvFrameRegion &second) const;
    /* Under the lock: `import` emptied once its last copy is done; false,
     * and kept, when it is not. */
    [[nodiscard]] bool forget(Import &import);
    void    drop_dead(uint64_t oldest_live);
    /* Emptied whatever its copies: only with the queue idle. */
    void    release(Import &import);

    const VulkanDeviceTable *table_;
    PosixLock                lock_;
    /* Declared before work_: the queue work waits on it as it closes. */
    VulkanTimeline           timeline_;
    QueueWork                work_;
    Import                   imports_[most_imports];
    uint64_t                 value_;
    uint64_t                 clock_;
    /* Threads inside wait(), and whether close() has begun: it lets go of the
     * timeline only when none is. */
    mutable uint32_t         waiters_;
    uint32_t                 closing_;
    uint32_t                 epoch_;

    friend struct FrameBufferCopyProbe;
};

}  // namespace av

#endif  // AV_FRAME_BUFFER_COPY_HPP
