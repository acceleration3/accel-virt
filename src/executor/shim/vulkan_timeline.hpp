/* A timeline semaphore on one device (VulkanDeviceTable): made there to be
 * exported - the compositor's - or made there over an fd another device
 * exported, so the device that writes an image and the one that reads it
 * order their work by one counter, on the GPU. OpenGL waits on the same fd
 * as a semaphore of its own (GL_EXT_semaphore_fd). */
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

    /* At 0 on `table`'s device, exportable as an OPAQUE_FD. */
    [[nodiscard]] bool make_exportable(const VulkanDeviceTable &table);
    /* Over the OPAQUE_FD `descriptor`, which is the timeline's once it is
     * made and stays held otherwise. */
    [[nodiscard]] bool import_fd(const VulkanDeviceTable &table, OwnedFd &descriptor);
    /* An fd another device imports, held in `descriptor`. */
    [[nodiscard]] bool export_fd(OwnedFd &descriptor) const;
    /* Waits until the counter reaches `value`, at most `nanoseconds`. */
    [[nodiscard]] bool wait(uint64_t value, uint64_t nanoseconds) const;
    /* Let go of; nothing may still wait on or signal it. */
    void               release();

    bool        made() const;
    VkSemaphore semaphore() const;

private:
    const VulkanDeviceTable *table_;
    VkSemaphore              semaphore_;
};

}  // namespace av

#endif  // AV_VULKAN_TIMELINE_HPP
