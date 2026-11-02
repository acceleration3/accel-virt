#include "device/present/region_mover.hpp"

#include "core/support/bgra_rows.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

namespace av {

RegionMover::RegionMover(const BlobWindow &source, const BlobWindow &destination, const PresentCopyRequest &request)
    : source_(source), destination_(destination), source_length_(source.length()),
      destination_length_(destination.length()), request_(request), chunk_{}
{
    AV_SCOPE("RegionMover::RegionMover");
    if (request.operation() != PresentOperation::color_fill) return;
    for (uint32_t &pixel : chunk_) pixel = request.colour();
}

Status RegionMover::move(const PresentRegion &region)
{
    AV_SCOPE("RegionMover::move");
    const bool     blt = request_.operation() == PresentOperation::blt;
    const uint64_t bytes = uint64_t{region.width()} * PresentRegion::pixel_bytes;
    for (uint32_t row = 0u; row < region.height(); row++) {
        uint64_t from = 0u, to = 0u;
        if (!offset_of(region.destination_x(), uint64_t{region.destination_y()} + row, request_.destination_pitch(),
                       destination_length_, to))
            return Status::invalid_parameter;
        if (blt && !offset_of(region.source_x(), uint64_t{region.source_y()} + row, request_.source_pitch(),
                              source_length_, from))
            return Status::invalid_parameter;
        const Status moved = move_row(from, to, bytes);
        if (moved != Status::ok) return moved;
    }
    return Status::ok;
}

Status RegionMover::move_row(uint64_t from, uint64_t to, uint64_t bytes)
{
    AV_SCOPE("RegionMover::move_row");
    const bool blt = request_.operation() == PresentOperation::blt;
    if (blt && !request_.swaps_red_and_blue()) return source_.copy_to(destination_, from, to, bytes);
    auto *const chunk = reinterpret_cast<uint8_t *>(chunk_);
    for (uint64_t done = 0u; done < bytes;) {
        const size_t piece = bytes - done < sizeof chunk_ ? static_cast<size_t>(bytes - done) : sizeof chunk_;
        const Status read = blt ? source_.read(from + done, chunk, piece) : Status::ok;
        if (read != Status::ok) return read;
        if (blt && request_.swaps_red_and_blue()) {
            BgraRows pixels(chunk, piece, piece);
            const Status swapped = pixels.copy_from(pixels, static_cast<uint32_t>(piece / PresentRegion::pixel_bytes),
                                                     1u, BgraRows::Channels::red_blue_swapped);
            if (swapped != Status::ok) return swapped;
        }
        const Status written = destination_.write(to + done, chunk, piece);
        if (written != Status::ok) return written;
        done += piece;
    }
    return Status::ok;
}

bool RegionMover::offset_of(uint64_t x, uint64_t y, uint32_t pitch, uint64_t length, uint64_t &offset) const
{
    AV_SCOPE("RegionMover::offset_of");
    if (pitch == 0u || y > length / pitch) return false;
    offset = y * pitch + x * PresentRegion::pixel_bytes;
    return true;
}

}  // namespace av
