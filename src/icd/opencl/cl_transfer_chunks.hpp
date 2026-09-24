/* A transfer between the application's memory and one of its buffers or
 * images, cut into commands the transport can carry.
 *
 * A write's bytes ride inside the command, in the staging room one command
 * has; a read's come back inside the reply slot. Both are fixed
 * (GuestClContext), and a command that overflows either is not a short
 * transfer but a faulted context - so a transfer of any size is sent as as many
 * commands as it takes, each within its room.
 *
 * Every command here asks the host to block. That is only safe because the
 * caller has already waited for the queue to drain: with nothing ahead of it,
 * the host's blocking call is the copy and nothing else, so no worker is held
 * behind a kernel. It is also what makes a write correct at all, because the
 * bytes a write carries live in the device's decode arena, which the next
 * command reuses - a host driver that read them later, when its queue reached
 * the write, would read the next command instead. */
#ifndef AV_CLICD_CL_TRANSFER_CHUNKS_HPP
#define AV_CLICD_CL_TRANSFER_CHUNKS_HPP

#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::clicd {

class ClImageRegion;

class ClTransferChunks {
public:
    /* What one command may carry each way, less room for the command's own
     * fields and the reply's: the handles, the offsets, the result, the
     * counts. Generous, because a chunk a little smaller costs nothing and one
     * a little larger costs the context. */
    static constexpr size_t write_room = GuestClContext::staging_bytes - 4096u;
    static constexpr size_t read_room = GuestClContext::reply_bytes - 4096u;

    ClTransferChunks(ClientTransport &wire, uint64_t queue);
    ClTransferChunks(const ClTransferChunks &) = delete;
    ClTransferChunks &operator=(const ClTransferChunks &) = delete;

    /* The first failure, or CL_SUCCESS once every chunk has gone. */
    cl_int read(uint64_t buffer, size_t offset, size_t bytes, void *into);
    cl_int write(uint64_t buffer, size_t offset, size_t bytes, const void *from);
    /* The same for an image, `region` saying where each row of it is. A run of
     * whole rows goes as one command where it fits, and a row too long for one
     * goes in pieces. A read into rows with a gap between them goes a row at a
     * time: what comes back is packed, and the gap is the application's
     * memory, not the host's to write. */
    cl_int read_image(uint64_t image, const ClImageRegion &region, void *into);
    cl_int write_image(uint64_t image, const ClImageRegion &region, const void *from);

    /* Commands sent, which is what a test counts a transfer's pieces by. */
    uint32_t commands() const;

private:
    /* Rows of `region` one command carries from row `row` on: as many as fit
     * `room` at `pitch` apart, at least one and no more than are left. Zero
     * when one row alone does not fit. */
    size_t rows_that_fit(const ClImageRegion &region, size_t row, size_t room, size_t pitch) const;
    /* One row of the region in pieces of whole texels that fit `room`. */
    cl_int read_row(uint64_t image, const ClImageRegion &region, size_t row, size_t slice,
                    uint8_t *into);
    cl_int write_row(uint64_t image, const ClImageRegion &region, size_t row, size_t slice,
                     const uint8_t *from);

    ClientTransport &wire_;
    uint64_t         queue_;
    uint32_t         commands_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_TRANSFER_CHUNKS_HPP
