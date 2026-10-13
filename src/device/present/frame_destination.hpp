/* Where the host's GPU writes a staged frame for a blt (write_frame,
 * docs/compositor.md): the destination surface's guest pages as one dma-buf,
 * rows `pitch` bytes apart, and the blt's regions as the executor names them.
 * The fd is the dma-buf's owner's (BlobDmaBuf); this only carries it. */
#ifndef AV_FRAME_DESTINATION_HPP
#define AV_FRAME_DESTINATION_HPP

#include "device/abi/av_executor.h"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <stdint.h>

namespace av {

class PresentRegion;

class FrameDestination {
public:
    FrameDestination(int fd, uint64_t bytes, uint32_t pitch);
    FrameDestination(const FrameDestination &) = delete;
    FrameDestination &operator=(const FrameDestination &) = delete;

    /* `region` among those written; false once max_regions are. */
    [[nodiscard]] bool add(const PresentRegion &region);

    int                  fd() const;
    uint64_t             bytes() const;
    uint32_t             pitch() const;
    const AvFrameRegion *regions() const;
    uint32_t             count() const;

private:
    int           fd_;
    uint64_t      bytes_;
    uint32_t      pitch_;
    AvFrameRegion regions_[PresentCopyRequest::max_regions];
    uint32_t      count_;
};

}  // namespace av

#endif  // AV_FRAME_DESTINATION_HPP
