#include "icd/opencl/cl_transfer_chunks.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/pieces.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/opencl/unanswered_transport.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_staged_rows.hpp"
#include "icd/opencl/cl_staging_block.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/resources/texel_layout.hpp"

namespace av::clicd {

ClTransferChunks::ClTransferChunks(ClientTransport &wire, uint64_t queue, ClArenaBlocks &blocks)
    : wire_(wire), queue_(queue), blocks_(blocks), commands_(0u)
{
    AV_SCOPE("ClTransferChunks::ClTransferChunks");
}

cl_int ClTransferChunks::read(uint64_t buffer, size_t offset, size_t bytes, void *into)
{
    AV_SCOPE("ClTransferChunks::read");
    uint8_t *const to = static_cast<uint8_t *>(into);
    ClStagingBlock staging(blocks_);
    if (staging.take(bytes)) return read_staged(staging, buffer, offset, bytes, to);
    return read_inline(buffer, offset, bytes, to);
}

cl_int ClTransferChunks::write(uint64_t buffer, size_t offset, size_t bytes, const void *from)
{
    AV_SCOPE("ClTransferChunks::write");
    const uint8_t *const source = static_cast<const uint8_t *>(from);
    ClStagingBlock       staging(blocks_);
    if (staging.take(bytes)) return write_staged(staging, buffer, offset, bytes, source);
    return write_inline(buffer, offset, bytes, source);
}

cl_int ClTransferChunks::read_image(uint64_t image, const ClImageRegion &region, void *into)
{
    AV_SCOPE("ClTransferChunks::read_image");
    uint8_t *const to = static_cast<uint8_t *>(into);
    ClStagingBlock staging(blocks_);
    if (staging.take(region.row_bytes() * region.rows() * region.slices()))
        return read_image_staged(staging, image, region, to);
    return read_image_inline(image, region, to);
}

cl_int ClTransferChunks::write_image(uint64_t image, const ClImageRegion &region, const void *from)
{
    AV_SCOPE("ClTransferChunks::write_image");
    const uint8_t *const source = static_cast<const uint8_t *>(from);
    ClStagingBlock       staging(blocks_);
    if (staging.take(region.row_bytes() * region.rows() * region.slices()))
        return write_image_staged(staging, image, region, source);
    return write_image_inline(image, region, source);
}

cl_int ClTransferChunks::read_buffer_rect(uint64_t buffer, const ClBufferRect &rect, void *into)
{
    AV_SCOPE("ClTransferChunks::read_buffer_rect");
    const ClImageRegion &application = rect.application();
    uint8_t *const       to = static_cast<uint8_t *>(into) + rect.host_offset();
    ClStagingBlock       staging(blocks_);
    if (staging.take(application.row_bytes() * application.rows() * application.slices()))
        return read_rect_staged(staging, buffer, rect, to);
    return read_rect_inline(buffer, rect, to);
}

cl_int ClTransferChunks::write_buffer_rect(uint64_t buffer, const ClBufferRect &rect, const void *from)
{
    AV_SCOPE("ClTransferChunks::write_buffer_rect");
    const ClImageRegion &application = rect.application();
    const uint8_t *const source = static_cast<const uint8_t *>(from) + rect.host_offset();
    ClStagingBlock       staging(blocks_);
    if (staging.take(application.row_bytes() * application.rows() * application.slices()))
        return write_rect_staged(staging, buffer, rect, source);
    return write_rect_inline(buffer, rect, source);
}

/* One piece a reply slot holds at a time, straight into the application's
 * memory. */
cl_int ClTransferChunks::read_inline(uint64_t buffer, size_t offset, size_t bytes, uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_inline");
    icd::Pieces pieces(bytes, read_room);
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

cl_int ClTransferChunks::write_inline(uint64_t buffer, size_t offset, size_t bytes, const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_inline");
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
cl_int ClTransferChunks::read_image_inline(uint64_t image, const ClImageRegion &region, uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_image_inline");
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
cl_int ClTransferChunks::write_image_inline(uint64_t image, const ClImageRegion &region, const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_image_inline");
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

/* As read_image: packed rows come back as one span and anything else a row at
 * a time, since what comes back is packed and a gap is the application's. */
cl_int ClTransferChunks::read_rect_inline(uint64_t buffer, const ClBufferRect &rect, uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_rect_inline");
    const ClImageRegion &buffer_side = rect.buffer_side();
    const ClImageRegion &application = rect.application();
    const TexelLayout bytes(1u, 1u, 1u);
    const size_t      most =
        application.rows_packed() || application.row_bytes() > read_room ? read_room : application.row_bytes();
    const size_t at_corner[3] = {};
    for (size_t slice = 0u; slice < application.slices(); ++slice) {
        uint8_t *const plane = to + application.row_offset(0u, slice);
        icd::RowPieces pieces(bytes, static_cast<uint32_t>(application.width()),
                              static_cast<uint32_t>(application.rows()),
                              static_cast<uint32_t>(application.row_pitch()), most);
        while (pieces.next()) {
            size_t origin[3] = {};
            buffer_side.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ++commands_;
            const cl_int read = av::clproto::av_clEnqueueReadBufferRect(
                wire_, queue_, buffer, CL_TRUE, origin, at_corner, extent, buffer_side.row_pitch(),
                buffer_side.slice_pitch(), 0u, 0u, plane + pieces.offset(), static_cast<size_t>(pieces.bytes()), 0u,
                nullptr, nullptr);
            if (read != CL_SUCCESS) return read;
        }
    }
    return CL_SUCCESS;
}

/* As write_image: rows with gaps go together at the application's pitch, and
 * a row cut into runs is one packed run. */
cl_int ClTransferChunks::write_rect_inline(uint64_t buffer, const ClBufferRect &rect, const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_rect_inline");
    const ClImageRegion &buffer_side = rect.buffer_side();
    const ClImageRegion &application = rect.application();
    const TexelLayout    bytes(1u, 1u, 1u);
    const size_t         at_corner[3] = {};
    for (size_t slice = 0u; slice < application.slices(); ++slice) {
        const uint8_t *const plane = source + application.row_offset(0u, slice);
        icd::RowPieces       pieces(bytes, static_cast<uint32_t>(application.width()),
                                    static_cast<uint32_t>(application.rows()),
                                    static_cast<uint32_t>(application.row_pitch()), write_room);
        while (pieces.next()) {
            size_t origin[3] = {};
            buffer_side.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            const bool   whole_rows = pieces.left() == 0u && pieces.right() == application.width();
            ++commands_;
            const cl_int written = av::clproto::av_clEnqueueWriteBufferRect(
                wire_, queue_, buffer, CL_TRUE, origin, at_corner, extent, buffer_side.row_pitch(),
                buffer_side.slice_pitch(), whole_rows ? application.row_pitch() : 0u, 0u, plane + pieces.offset(),
                static_cast<size_t>(pieces.bytes()), 0u, nullptr, nullptr);
            if (written != CL_SUCCESS) return written;
        }
    }
    return CL_SUCCESS;
}

/* A block's worth at a time: the host writes it into the block, and it is
 * copied out before the block takes the next. */
cl_int ClTransferChunks::read_staged(const ClStagingBlock &staging, uint64_t buffer, size_t offset, size_t bytes,
                                     uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_staged");
    icd::Pieces pieces(bytes, staging.bytes());
    while (pieces.next()) {
        const size_t count = static_cast<size_t>(pieces.count());
        ++commands_;
        const cl_int read = av::clproto::av_clEnqueueReadBuffer(wire_, queue_, buffer, CL_TRUE,
                                                                 offset + static_cast<size_t>(pieces.offset()), count,
                                                                 staging.at(), 0u, nullptr, nullptr);
        if (read != CL_SUCCESS) return read;
        __builtin_memcpy(to + pieces.offset(), staging.at(), count);
    }
    return CL_SUCCESS;
}

/* Each piece answered before the next is copied over it: the host reads the
 * block when it runs the command, which may be after its send. */
cl_int ClTransferChunks::write_staged(const ClStagingBlock &staging, uint64_t buffer, size_t offset, size_t bytes,
                                      const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_staged");
    icd::Pieces pieces(bytes, staging.bytes());
    while (pieces.next()) {
        const size_t count = static_cast<size_t>(pieces.count());
        __builtin_memcpy(staging.at(), source + pieces.offset(), count);
        ++commands_;
        const cl_int written = av::clproto::av_clEnqueueWriteBuffer(
            wire_, queue_, buffer, CL_TRUE, offset + static_cast<size_t>(pieces.offset()), count, staging.at(), 0u,
            nullptr, nullptr);
        if (written != CL_SUCCESS) return written;
    }
    return CL_SUCCESS;
}

/* Rows packed tight in the block whatever the application's pitch, so a piece
 * is as many whole rows as the block holds, or a run of one, and names no
 * pitch at all. */
cl_int ClTransferChunks::read_image_staged(const ClStagingBlock &staging, uint64_t image,
                                           const ClImageRegion &region, uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_image_staged");
    const TexelLayout texels(static_cast<uint32_t>(region.element_bytes()), 1u, 1u);
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        icd::RowPieces pieces(texels, static_cast<uint32_t>(region.width()), static_cast<uint32_t>(region.rows()),
                              static_cast<uint32_t>(region.row_bytes()), staging.bytes());
        while (pieces.next()) {
            size_t origin[3] = {};
            region.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ++commands_;
            const cl_int read = av::clproto::av_clEnqueueReadImage(wire_, queue_, image, CL_TRUE, origin, extent, 0u,
                                                                    0u, staging.at(),
                                                                    static_cast<size_t>(pieces.bytes()), 0u, nullptr,
                                                                    nullptr);
            if (read != CL_SUCCESS) return read;
            ClStagedRows(region, pieces, slice).unpack(staging.at(), to);
        }
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::write_image_staged(const ClStagingBlock &staging, uint64_t image,
                                            const ClImageRegion &region, const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_image_staged");
    const TexelLayout texels(static_cast<uint32_t>(region.element_bytes()), 1u, 1u);
    for (size_t slice = 0u; slice < region.slices(); ++slice) {
        icd::RowPieces pieces(texels, static_cast<uint32_t>(region.width()), static_cast<uint32_t>(region.rows()),
                              static_cast<uint32_t>(region.row_bytes()), staging.bytes());
        while (pieces.next()) {
            size_t origin[3] = {};
            region.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ClStagedRows(region, pieces, slice).pack(source, staging.at());
            ++commands_;
            const cl_int written = av::clproto::av_clEnqueueWriteImage(
                wire_, queue_, image, CL_TRUE, origin, extent, 0u, 0u, staging.at(),
                static_cast<size_t>(pieces.bytes()), 0u, nullptr, nullptr);
            if (written != CL_SUCCESS) return written;
        }
    }
    return CL_SUCCESS;
}

/* As an image's, in bytes: the buffer's side at its own pitches, the block's
 * packed from its start. */
cl_int ClTransferChunks::read_rect_staged(const ClStagingBlock &staging, uint64_t buffer, const ClBufferRect &rect,
                                          uint8_t *to)
{
    AV_SCOPE("ClTransferChunks::read_rect_staged");
    const ClImageRegion &buffer_side = rect.buffer_side();
    const ClImageRegion &application = rect.application();
    const TexelLayout    bytes(1u, 1u, 1u);
    const size_t         at_corner[3] = {};
    for (size_t slice = 0u; slice < application.slices(); ++slice) {
        icd::RowPieces pieces(bytes, static_cast<uint32_t>(application.width()),
                              static_cast<uint32_t>(application.rows()),
                              static_cast<uint32_t>(application.row_bytes()), staging.bytes());
        while (pieces.next()) {
            size_t origin[3] = {};
            buffer_side.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ++commands_;
            const cl_int read = av::clproto::av_clEnqueueReadBufferRect(
                wire_, queue_, buffer, CL_TRUE, origin, at_corner, extent, buffer_side.row_pitch(),
                buffer_side.slice_pitch(), 0u, 0u, staging.at(), static_cast<size_t>(pieces.bytes()), 0u, nullptr,
                nullptr);
            if (read != CL_SUCCESS) return read;
            ClStagedRows(application, pieces, slice).unpack(staging.at(), to);
        }
    }
    return CL_SUCCESS;
}

cl_int ClTransferChunks::write_rect_staged(const ClStagingBlock &staging, uint64_t buffer, const ClBufferRect &rect,
                                           const uint8_t *source)
{
    AV_SCOPE("ClTransferChunks::write_rect_staged");
    const ClImageRegion &buffer_side = rect.buffer_side();
    const ClImageRegion &application = rect.application();
    const TexelLayout    bytes(1u, 1u, 1u);
    const size_t         at_corner[3] = {};
    for (size_t slice = 0u; slice < application.slices(); ++slice) {
        icd::RowPieces pieces(bytes, static_cast<uint32_t>(application.width()),
                              static_cast<uint32_t>(application.rows()),
                              static_cast<uint32_t>(application.row_bytes()), staging.bytes());
        while (pieces.next()) {
            size_t origin[3] = {};
            buffer_side.origin_of(pieces.left(), pieces.top(), slice, origin);
            const size_t extent[3] = {pieces.right() - pieces.left(), pieces.bottom() - pieces.top(), 1u};
            ClStagedRows(application, pieces, slice).pack(source, staging.at());
            ++commands_;
            const cl_int written = av::clproto::av_clEnqueueWriteBufferRect(
                wire_, queue_, buffer, CL_TRUE, origin, at_corner, extent, buffer_side.row_pitch(),
                buffer_side.slice_pitch(), 0u, 0u, staging.at(), static_cast<size_t>(pieces.bytes()), 0u, nullptr,
                nullptr);
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
