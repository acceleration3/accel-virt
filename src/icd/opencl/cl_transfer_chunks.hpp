/* A transfer between the application's memory and one of its buffers or
 * images, cut into commands the transport can carry.
 *
 * Staged where the context's arena has a block to spare (ClStagingBlock): the
 * application's bytes are copied into the block, or out of it, and each
 * command names the block by offset - a lent parameter - so the host reads
 * and writes them in place. A rectangle or an image region is packed tight in
 * the block, whatever the application's pitches, so its gaps cost nothing.
 * One command per block's worth, each answered before the block is reused.
 *
 * Inline otherwise: a write's bytes ride inside the command, in the staging
 * room one command has; a read's come back inside the reply slot. Both are
 * fixed (GuestClContext), and a command that overflows either is not a short
 * transfer but a faulted context - so a transfer of any size is sent as as
 * many commands as it takes, each within its room.
 *
 * Every command here asks the host to block. That is only safe because the
 * caller has already waited for the queue to drain: with nothing ahead of it,
 * the host's blocking call is the copy and nothing else, so no worker is held
 * behind a kernel. It is also what makes a transfer correct at all, because
 * its bytes live in the device's decode arena, which the next command reuses,
 * or in a block that goes back to the arena when the transfer returns - a host
 * driver that touched them later, when its queue reached the transfer, would
 * find something else there. */
#ifndef AV_CLICD_CL_TRANSFER_CHUNKS_HPP
#define AV_CLICD_CL_TRANSFER_CHUNKS_HPP

#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::icd {
class RowPieces;
}  // namespace av::icd

namespace av::clicd {

class ClArenaBlocks;
class ClBufferRect;
class ClImageRegion;
class ClStagingBlock;

class ClTransferChunks {
public:

    /* `blocks` is the context's arena, which transfers are staged in. */
    ClTransferChunks(ClientTransport &wire, uint64_t queue, ClArenaBlocks &blocks);
    ClTransferChunks(const ClTransferChunks &) = delete;
    ClTransferChunks &operator=(const ClTransferChunks &) = delete;

    /* The first failure, or CL_SUCCESS once every piece has gone. */
    cl_int read(uint64_t buffer, size_t offset, size_t bytes, void *into);
    cl_int write(uint64_t buffer, size_t offset, size_t bytes, const void *from);
    /* The same for an image, `region` saying where each row of it is, in
     * RowPieces: a run of whole rows as one command where it fits, and a row
     * too long for one in runs of whole texels. Inline, a read into rows with
     * a gap between them goes a row at a time: what comes back is packed, and
     * the gap is the application's memory, not the host's to write. */
    cl_int read_image(uint64_t image, const ClImageRegion &region, void *into);
    cl_int write_image(uint64_t image, const ClImageRegion &region, const void *from);
    /* A rectangle of a buffer (checked), cut as an image's rows are, from the
     * application's corner of it in `into` or `from`. Each piece is a
     * rectangle of its own at the buffer's pitches, its bytes packed from its
     * own corner. */
    cl_int read_buffer_rect(uint64_t buffer, const ClBufferRect &rect, void *into);
    cl_int write_buffer_rect(uint64_t buffer, const ClBufferRect &rect, const void *from);

    /* Commands sent, which is what a test counts a transfer's pieces by. */
    uint32_t commands() const;

private:
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

}  // namespace av::clicd

#endif  // AV_CLICD_CL_TRANSFER_CHUNKS_HPP
