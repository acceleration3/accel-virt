/* One DxgkDdiBuildPagingBuffer call. An aperture segment over guest memory
 * needs no paging commands: mapping, unmapping and discarding an allocation's
 * pages change nothing the device reads, so those succeed with an empty buffer.
 * VidMm issues FILL and TRANSFER for memory segments (TransferToSegment), which
 * this adapter does not report, so every other operation is refused. */
#ifndef AV_PAGING_REQUEST_HPP
#define AV_PAGING_REQUEST_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargBuildPagingBufferRecord;

class PagingRequest {
public:
    PagingRequest(Platform &platform, DxgkargBuildPagingBufferRecord &request);
    PagingRequest(const PagingRequest &) = delete;
    PagingRequest &operator=(const PagingRequest &) = delete;

    [[nodiscard]] Status build();

private:
    /* DXGK_BUILDPAGINGBUFFER_OPERATION */
    enum : int32_t { discard_content = 2, map_aperture_segment = 5, unmap_aperture_segment = 6 };

    Platform                       &platform_;
    DxgkargBuildPagingBufferRecord &request_;
};

}  // namespace av

#endif  // AV_PAGING_REQUEST_HPP
