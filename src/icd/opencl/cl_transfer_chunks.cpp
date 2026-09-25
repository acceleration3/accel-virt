#include "icd/opencl/cl_transfer_chunks.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/unanswered_transport.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClTransferChunks::ClTransferChunks(ClientTransport &wire, uint64_t queue)
    : wire_(wire), queue_(queue), commands_(0u)
{
    AV_SCOPE("ClTransferChunks::ClTransferChunks");
}

cl_int ClTransferChunks::read(uint64_t buffer, size_t offset, size_t bytes, void *into)
{
    AV_SCOPE("ClTransferChunks::read");
    uint8_t *const to = static_cast<uint8_t *>(into);
    for (size_t done = 0u; done < bytes;) {
        const size_t piece = bytes - done < read_room ? bytes - done : read_room;
        ++commands_;
        const cl_int read = av::clproto::rvk_clEnqueueReadBuffer(
            wire_, queue_, buffer, CL_TRUE, offset + done, piece, to + done, 0u, nullptr, nullptr);
        if (read != CL_SUCCESS) return read;
        done += piece;
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::write(uint64_t buffer, size_t offset, size_t bytes, const void *from)
{
    AV_SCOPE("ClTransferChunks::write");
    const uint8_t *const source = static_cast<const uint8_t *>(from);
    /* The host makes every write blocking, in queue order, before the next
     * command, so a piece needs no answer of its own: the caller's drain after
     * the last one is the answer for all of them. */
    icd::UnansweredTransport unanswered(wire_);
    for (size_t done = 0u; done < bytes;) {
        const size_t piece = bytes - done < write_room ? bytes - done : write_room;
        ++commands_;
        const cl_int written = av::clproto::rvk_clEnqueueWriteBuffer(
            unanswered, queue_, buffer, CL_TRUE, offset + done, piece, source + done, 0u, nullptr,
            nullptr);
        if (written != CL_SUCCESS) return written;
        done += piece;
    }
    return CL_SUCCESS;
}

size_t ClTransferChunks::rows_that_fit(const ClImageRegion &region, size_t row, size_t room,
                                       size_t pitch) const
{
    AV_SCOPE("ClTransferChunks::rows_that_fit");
    if (region.row_bytes() > room) return 0u;
    /* The last row of a run needs only its own bytes; every one before it
     * needs a whole pitch. */
    const size_t fits = 1u + (room - region.row_bytes()) / pitch;
    const size_t left = region.rows() - row;
    return fits < left ? fits : left;
}

cl_int ClTransferChunks::read_row(uint64_t image, const ClImageRegion &region, size_t row,
                                  size_t slice, uint8_t *into)
{
    AV_SCOPE("ClTransferChunks::read_row");
    const size_t texels = read_room / region.element_bytes();
    for (size_t x = 0u; x < region.width(); x += texels) {
        const size_t count = region.width() - x < texels ? region.width() - x : texels;
        size_t       origin[3] = {};
        region.origin_of(x, row, slice, origin);
        const size_t extent[3] = {count, 1u, 1u};
        ++commands_;
        const cl_int read = av::clproto::rvk_clEnqueueReadImage(
            wire_, queue_, image, CL_TRUE, origin, extent, 0u, 0u,
            into + x * region.element_bytes(), count * region.element_bytes(), 0u, nullptr, nullptr);
        if (read != CL_SUCCESS) return read;
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::write_row(uint64_t image, const ClImageRegion &region, size_t row,
                                   size_t slice, const uint8_t *from)
{
    AV_SCOPE("ClTransferChunks::write_row");
    const size_t texels = write_room / region.element_bytes();
    for (size_t x = 0u; x < region.width(); x += texels) {
        const size_t count = region.width() - x < texels ? region.width() - x : texels;
        size_t       origin[3] = {};
        region.origin_of(x, row, slice, origin);
        const size_t extent[3] = {count, 1u, 1u};
        ++commands_;
        const cl_int written = av::clproto::rvk_clEnqueueWriteImage(
            wire_, queue_, image, CL_TRUE, origin, extent, 0u, 0u,
            from + x * region.element_bytes(), count * region.element_bytes(), 0u, nullptr, nullptr);
        if (written != CL_SUCCESS) return written;
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::read_image(uint64_t image, const ClImageRegion &region, void *into)
{
    AV_SCOPE("ClTransferChunks::read_image");
    uint8_t *const to = static_cast<uint8_t *>(into);
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        for (size_t row = 0u; row < region.rows();) {
            uint8_t *const at = to + region.row_offset(row, slice);
            /* Packed rows come back as one span; anything else a row at a
             * time, so the gaps are never written. */
            size_t run = rows_that_fit(region, row, read_room, region.row_bytes());
            if (!region.rows_packed() && run > 1u) run = 1u;
            if (run == 0u) {
                const cl_int pieces = read_row(image, region, row, slice, at);
                if (pieces != CL_SUCCESS) return pieces;
                ++row;
                continue;
            }
            size_t origin[3] = {};
            region.origin_of(0u, row, slice, origin);
            const size_t extent[3] = {region.width(), run, 1u};
            ++commands_;
            const cl_int read = av::clproto::rvk_clEnqueueReadImage(
                wire_, queue_, image, CL_TRUE, origin, extent, 0u, 0u, at, run * region.row_bytes(),
                0u, nullptr, nullptr);
            if (read != CL_SUCCESS) return read;
            row += run;
        }
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::write_image(uint64_t image, const ClImageRegion &region, const void *from)
{
    AV_SCOPE("ClTransferChunks::write_image");
    const uint8_t *const source = static_cast<const uint8_t *>(from);
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        for (size_t row = 0u; row < region.rows();) {
            const uint8_t *const at = source + region.row_offset(row, slice);
            const size_t run = rows_that_fit(region, row, write_room, region.row_pitch());
            if (run == 0u) {
                const cl_int pieces = write_row(image, region, row, slice, at);
                if (pieces != CL_SUCCESS) return pieces;
                ++row;
                continue;
            }
            /* Rows with gaps between them go together too: the gaps are read,
             * which the application's memory allows, and the host skips them
             * by the pitch. */
            size_t origin[3] = {};
            region.origin_of(0u, row, slice, origin);
            const size_t extent[3] = {region.width(), run, 1u};
            const size_t span = (run - 1u) * region.row_pitch() + region.row_bytes();
            ++commands_;
            const cl_int written = av::clproto::rvk_clEnqueueWriteImage(
                wire_, queue_, image, CL_TRUE, origin, extent, region.row_pitch(), 0u, at, span, 0u,
                nullptr, nullptr);
            if (written != CL_SUCCESS) return written;
            row += run;
        }
    }
    return CL_SUCCESS;
}

uint32_t ClTransferChunks::commands() const
{
    AV_SCOPE("ClTransferChunks::commands");
    return commands_;
}

}  // namespace av::clicd
