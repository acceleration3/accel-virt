#include "proto/virtio_gpu/queued_present_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

QueuedPresentRequest::QueuedPresentRequest() : ticket_(0u), source_(Source::pages), image_(), copy_()
{
    AV_SCOPE("QueuedPresentRequest::QueuedPresentRequest");
}

QueuedPresentRequest::QueuedPresentRequest(uint64_t ticket, const PresentCopyRequest &copy)
    : ticket_(ticket), source_(Source::pages), image_(), copy_(copy)
{
    AV_SCOPE("QueuedPresentRequest::QueuedPresentRequest");
}

QueuedPresentRequest::QueuedPresentRequest(uint64_t ticket, const SurfacePoint &image, const PresentCopyRequest &copy)
    : ticket_(ticket), source_(Source::image), image_(image), copy_(copy)
{
    AV_SCOPE("QueuedPresentRequest::QueuedPresentRequest");
}

void QueuedPresentRequest::write(WireWriter &writer) const
{
    AV_SCOPE("QueuedPresentRequest::write");
    writer.write_u32(type);
    writer.write_zeros(header_size - sizeof(uint32_t));
    writer.write_u64(ticket_);
    writer.write_u32(static_cast<uint32_t>(source_));
    writer.write_u32(image_.point());
    writer.write_u64(image_.surface());
    writer.write_u32(static_cast<uint32_t>(copy_.operation()));
    writer.write_u32(copy_.swaps_red_and_blue() ? PresentCopyRequest::flag_swap_red_blue : 0u);
    writer.write_u32(copy_.colour());
    writer.write_u32(copy_.regions());
    writer.write_u32(copy_.source_entries());
    writer.write_u32(copy_.destination_entries());
    writer.write_u32(copy_.source_pitch());
    writer.write_u32(copy_.destination_pitch());
}

Status QueuedPresentRequest::decode(WireReader &reader)
{
    AV_SCOPE("QueuedPresentRequest::decode");
    uint8_t header[header_size] = {};
    reader.copy_bytes(header, sizeof header);
    QueuedPresentRequest read;
    read.ticket_ = reader.read_u64();
    read.source_ = static_cast<Source>(reader.read_u32());
    const uint32_t point = reader.read_u32();
    const uint64_t surface = reader.read_u64();
    read.image_ = SurfacePoint(surface, point);
    const auto     operation = static_cast<PresentOperation>(reader.read_u32());
    const uint32_t flags = reader.read_u32();
    const uint32_t colour = reader.read_u32();
    const uint32_t regions = reader.read_u32();
    const uint32_t source_entries = reader.read_u32();
    const uint32_t destination_entries = reader.read_u32();
    const uint32_t source_pitch = reader.read_u32();
    const uint32_t destination_pitch = reader.read_u32();
    if (reader.status() != Status::ok || (flags & ~PresentCopyRequest::flag_swap_red_blue) != 0u)
        return Status::malformed;
    read.copy_ = PresentCopyRequest(operation, flags, colour, regions, source_entries, source_pitch,
                                    destination_entries, destination_pitch);
    if (!read.holds()) return Status::malformed;
    *this = read;
    return Status::ok;
}

bool QueuedPresentRequest::holds() const
{
    AV_SCOPE("QueuedPresentRequest::holds");
    if (ticket_ == 0u) return false;
    const bool blt = copy_.operation() == PresentOperation::blt;
    if (!blt && copy_.operation() != PresentOperation::color_fill) return false;
    if (!blt && copy_.swaps_red_and_blue()) return false;
    if (blt && copy_.colour() != 0u) return false;
    if (copy_.regions() == 0u || copy_.regions() > max_regions) return false;
    if (copy_.destination_entries() == 0u || copy_.destination_entries() > max_entries) return false;
    if (copy_.destination_pitch() == 0u || copy_.destination_pitch() % pixel_bytes != 0u) return false;
    const bool no_source = copy_.source_entries() == 0u && copy_.source_pitch() == 0u;
    if (source_ == Source::image) return blt && image_.names() && no_source;
    if (source_ != Source::pages || !(image_ == SurfacePoint())) return false;
    if (!blt) return no_source;
    return copy_.source_entries() != 0u && copy_.source_entries() <= max_entries && copy_.source_pitch() != 0u &&
           copy_.source_pitch() % pixel_bytes == 0u;
}

uint64_t QueuedPresentRequest::ticket() const
{
    AV_SCOPE("QueuedPresentRequest::ticket");
    return ticket_;
}

QueuedPresentRequest::Source QueuedPresentRequest::source() const
{
    AV_SCOPE("QueuedPresentRequest::source");
    return source_;
}

const SurfacePoint &QueuedPresentRequest::image() const
{
    AV_SCOPE("QueuedPresentRequest::image");
    return image_;
}

const PresentCopyRequest &QueuedPresentRequest::copy() const
{
    AV_SCOPE("QueuedPresentRequest::copy");
    return copy_;
}

size_t QueuedPresentRequest::destination_entries_offset() const
{
    AV_SCOPE("QueuedPresentRequest::destination_entries_offset");
    return encoded_size + size_t{copy_.source_entries()} * entry_size;
}

size_t QueuedPresentRequest::regions_offset() const
{
    AV_SCOPE("QueuedPresentRequest::regions_offset");
    return destination_entries_offset() + size_t{copy_.destination_entries()} * entry_size;
}

size_t QueuedPresentRequest::size() const
{
    AV_SCOPE("QueuedPresentRequest::size");
    return regions_offset() + size_t{copy_.regions()} * PresentRegion::encoded_size;
}

}
