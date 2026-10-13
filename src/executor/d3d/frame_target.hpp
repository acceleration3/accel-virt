/* Where write_frame puts a staged frame (docs/compositor.md): a surface's
 * guest pages as one dma-buf, rows `pitch` bytes apart, and the rectangles the
 * blt names - which the guest chose, so each is judged against the frame and
 * the surface before the GPU is told to write anything. */
#ifndef AV_FRAME_TARGET_HPP
#define AV_FRAME_TARGET_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class FrameTarget {
public:
    FrameTarget(int fd, uint64_t bytes, uint32_t pitch, const AvFrameRegion *regions, uint32_t count);
    FrameTarget(const FrameTarget &) = delete;
    FrameTarget &operator=(const FrameTarget &) = delete;

    /* The dma-buf: the caller's, never closed here. */
    int      fd() const;
    uint64_t bytes() const;
    uint32_t count() const;
    uint32_t pitch() const;
    const AvFrameRegion &region(uint32_t index) const;
    /* Whether every region is inside a `width` by `height` frame of four-byte
     * pixels and inside the surface, and there is at least one. */
    bool              fits(uint32_t width, uint32_t height) const;
    /* Region `index` as a copy from the frame's image into the surface. */
    VkBufferImageCopy copy(uint32_t index) const;

private:
    bool inside(const AvFrameRegion &region, uint32_t width, uint32_t height) const;

    int                  fd_;
    uint64_t             bytes_;
    uint32_t             pitch_;
    const AvFrameRegion *regions_;
    uint32_t             count_;
};

}  // namespace av

#endif  // AV_FRAME_TARGET_HPP
