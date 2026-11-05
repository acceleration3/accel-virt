#ifndef AV_CLICD_CL_TRANSFER_CHUNKS_HPP
#define AV_CLICD_CL_TRANSFER_CHUNKS_HPP

#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {
class RowPieces;
}

namespace av::clicd {

class ClArenaBlocks;
class ClBufferRect;
class ClImageRegion;
class ClStagingBlock;

class ClTransferChunks {
public:

    ClTransferChunks(ClientTransport &wire, uint64_t queue, ClArenaBlocks &blocks);
    ClTransferChunks(const ClTransferChunks &) = delete;
    ClTransferChunks &operator=(const ClTransferChunks &) = delete;

    cl_int read(uint64_t buffer, size_t offset, size_t bytes, void *into);
    cl_int write(uint64_t buffer, size_t offset, size_t bytes, const void *from);
    cl_int read_image(uint64_t image, const ClImageRegion &region, void *into);
    cl_int write_image(uint64_t image, const ClImageRegion &region, const void *from);
    cl_int read_buffer_rect(uint64_t buffer, const ClBufferRect &rect, void *into);
    cl_int write_buffer_rect(uint64_t buffer, const ClBufferRect &rect, const void *from);

    uint32_t commands() const;

private:
    [[nodiscard]] bool take_staging(ClStagingBlock &staging, uint64_t wanted, uint64_t smallest);
    cl_int read_inline(uint64_t buffer, size_t offset, size_t bytes, uint8_t *to);
    cl_int write_inline(uint64_t buffer, size_t offset, size_t bytes, const uint8_t *source);
    cl_int read_image_inline(uint64_t image, const ClImageRegion &region, uint8_t *to);
    cl_int write_image_inline(uint64_t image, const ClImageRegion &region, const uint8_t *source);
    cl_int read_rect_inline(uint64_t buffer, const ClBufferRect &rect, uint8_t *to);
    cl_int write_rect_inline(uint64_t buffer, const ClBufferRect &rect, const uint8_t *source);

    cl_int read_staged(const ClStagingBlock &staging, uint64_t buffer, size_t offset, size_t bytes, uint8_t *to);
    cl_int write_staged(const ClStagingBlock &staging, uint64_t buffer, size_t offset, size_t bytes,
                        const uint8_t *source);
    cl_int read_image_staged(const ClStagingBlock &staging, uint64_t image, const ClImageRegion &region,
                             uint8_t *to);
    cl_int write_image_staged(const ClStagingBlock &staging, uint64_t image, const ClImageRegion &region,
                              const uint8_t *source);
    cl_int read_rect_staged(const ClStagingBlock &staging, uint64_t buffer, const ClBufferRect &rect, uint8_t *to);
    cl_int write_rect_staged(const ClStagingBlock &staging, uint64_t buffer, const ClBufferRect &rect,
                             const uint8_t *source);

    ClientTransport &wire_;
    uint64_t         queue_;
    ClArenaBlocks   &blocks_;
    uint32_t         commands_;
};

}

#endif
