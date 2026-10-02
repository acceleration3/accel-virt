#include "icd/opencl/cl_transfer_chunks.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/pieces.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/common/unanswered_transport.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/resources/texel_layout.hpp"

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
    icd::Pieces    pieces(bytes, read_room);
    while (pieces.next()) {
        ++commands_;
        const cl_int read = av::clproto::av_clEnqueueReadBuffer(wire_, queue_, buffer, CL_TRUE,
                                                                 offset + static_cast<size_t>(pieces.offset()),
                                                                 static_cast<size_t>(pieces.count()),
                                                                 to + pieces.offset(), 0u, nullptr, nullptr);
        if (read != CL_SUCCESS) return read;
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
    icd::Pieces              pieces(bytes, write_room);
    while (pieces.next()) {
        ++commands_;
        const cl_int written = av::clproto::av_clEnqueueWriteBuffer(
            unanswered, queue_, buffer, CL_TRUE, offset + static_cast<size_t>(pieces.offset()),
            static_cast<size_t>(pieces.count()), source + pieces.offset(), 0u, nullptr, nullptr);
        if (written != CL_SUCCESS) return written;
    }
    return CL_SUCCESS;
}

/* Packed rows come back as one span; anything else a row at a time, so the
 * gaps between them - the application's memory, not the host's to write -
 * are never written: a piece no larger than a row holds no more than one. */
cl_int ClTransferChunks::read_image(uint64_t image, const ClImageRegion &region, void *into)
{
    AV_SCOPE("ClTransferChunks::read_image");
    uint8_t *const    to = static_cast<uint8_t *>(into);
    const TexelLayout texels(static_cast<uint32_t>(region.element_bytes()), 1u, 1u);
    const size_t      most = region.rows_packed() || region.row_bytes() > read_room ? read_room : region.row_bytes();
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        uint8_t *const plane = to + region.row_offset(0u, slice);
        icd::RowPieces pieces(texels, static_cast<uint32_t>(region.width()), static_cast<uint32_t>(region.rows()),
                              static_cast<uint32_t>(region.row_pitch()), most);
        while (pieces.next()) {
            size_t origin[3] = {};
            region.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ++commands_;
            const cl_int read = av::clproto::av_clEnqueueReadImage(wire_, queue_, image, CL_TRUE, origin, extent, 0u,
                                                                    0u, plane + pieces.offset(),
                                                                    static_cast<size_t>(pieces.bytes()), 0u, nullptr,
                                                                    nullptr);
            if (read != CL_SUCCESS) return read;
        }
    }
    return CL_SUCCESS;
}

/* Rows with gaps between them go together: the gaps are read, which the
 * application's memory allows, and the host skips them by the pitch. A row cut
 * into runs is one run of packed texels, which names no pitch at all. */
cl_int ClTransferChunks::write_image(uint64_t image, const ClImageRegion &region, const void *from)
{
    AV_SCOPE("ClTransferChunks::write_image");
    const uint8_t *const source = static_cast<const uint8_t *>(from);
    const TexelLayout    texels(static_cast<uint32_t>(region.element_bytes()), 1u, 1u);
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        const uint8_t *const plane = source + region.row_offset(0u, slice);
        icd::RowPieces       pieces(texels, static_cast<uint32_t>(region.width()),
                                    static_cast<uint32_t>(region.rows()), static_cast<uint32_t>(region.row_pitch()),
                                    write_room);
        while (pieces.next()) {
            size_t origin[3] = {};
            region.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            const bool   whole_rows = pieces.left() == 0u && pieces.right() == region.width();
            ++commands_;
            const cl_int written = av::clproto::av_clEnqueueWriteImage(
                wire_, queue_, image, CL_TRUE, origin, extent, whole_rows ? region.row_pitch() : 0u, 0u,
                plane + pieces.offset(), static_cast<size_t>(pieces.bytes()), 0u, nullptr, nullptr);
            if (written != CL_SUCCESS) return written;
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
