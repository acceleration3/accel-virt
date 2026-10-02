#include "proto/virtio_gpu/present_copy_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentCopyRequest::PresentCopyRequest()
    : operation_(PresentOperation::blt), flags_(0u), colour_(0u), regions_(0u), source_entries_(0u),
      source_pitch_(0u), destination_entries_(0u), destination_pitch_(0u)
{
    AV_SCOPE("PresentCopyRequest::PresentCopyRequest");
}

PresentCopyRequest::PresentCopyRequest(PresentOperation operation, uint32_t flags, uint32_t colour, uint32_t regions,
                                       uint32_t source_entries, uint32_t source_pitch, uint32_t destination_entries,
                                       uint32_t destination_pitch)
    : operation_(operation), flags_(flags), colour_(colour), regions_(regions), source_entries_(source_entries),
      source_pitch_(source_pitch), destination_entries_(destination_entries), destination_pitch_(destination_pitch)
{
    AV_SCOPE("PresentCopyRequest::PresentCopyRequest");
}

void PresentCopyRequest::write(WireWriter &writer) const
{
    AV_SCOPE("PresentCopyRequest::write");
    writer.write_u32(type);
    writer.write_zeros(header_size - sizeof(uint32_t));
    writer.write_u32(static_cast<uint32_t>(operation_));
    writer.write_u32(flags_);
    writer.write_u32(colour_);
    writer.write_u32(regions_);
    writer.write_u32(source_entries_);
    writer.write_u32(destination_entries_);
    writer.write_u32(source_pitch_);
    writer.write_u32(destination_pitch_);
}

Status PresentCopyRequest::decode(WireReader &reader)
{
    AV_SCOPE("PresentCopyRequest::decode");
    uint8_t header[header_size] = {};
    reader.copy_bytes(header, sizeof header);
    PresentCopyRequest read;
    read.operation_ = static_cast<PresentOperation>(reader.read_u32());
    read.flags_ = reader.read_u32();
    read.colour_ = reader.read_u32();
    read.regions_ = reader.read_u32();
    read.source_entries_ = reader.read_u32();
    read.destination_entries_ = reader.read_u32();
    read.source_pitch_ = reader.read_u32();
    read.destination_pitch_ = reader.read_u32();
    if (reader.status() != Status::ok || !read.holds()) return Status::malformed;
    *this = read;
    return Status::ok;
}

bool PresentCopyRequest::holds() const
{
    AV_SCOPE("PresentCopyRequest::holds");
    const bool blt = operation_ == PresentOperation::blt;
    if (!blt && operation_ != PresentOperation::color_fill) return false;
    const uint32_t flags_taken = blt ? flag_swap_red_blue : 0u;
    if ((flags_ & ~flags_taken) != 0u || (blt && colour_ != 0u)) return false;
    if (regions_ == 0u || regions_ > max_regions) return false;
    if (destination_entries_ == 0u || destination_entries_ > max_entries) return false;
    if (destination_pitch_ == 0u || destination_pitch_ % pixel_bytes != 0u) return false;
    if (!blt) return source_entries_ == 0u && source_pitch_ == 0u;
    return source_entries_ != 0u && source_entries_ <= max_entries && source_pitch_ != 0u &&
           source_pitch_ % pixel_bytes == 0u;
}

PresentOperation PresentCopyRequest::operation() const
{
    AV_SCOPE("PresentCopyRequest::operation");
    return operation_;
}

bool PresentCopyRequest::swaps_red_and_blue() const
{
    AV_SCOPE("PresentCopyRequest::swaps_red_and_blue");
    return (flags_ & flag_swap_red_blue) != 0u;
}

uint32_t PresentCopyRequest::colour() const
{
    AV_SCOPE("PresentCopyRequest::colour");
    return colour_;
}

uint32_t PresentCopyRequest::regions() const
{
    AV_SCOPE("PresentCopyRequest::regions");
    return regions_;
}

uint32_t PresentCopyRequest::source_entries() const
{
    AV_SCOPE("PresentCopyRequest::source_entries");
    return source_entries_;
}

uint32_t PresentCopyRequest::source_pitch() const
{
    AV_SCOPE("PresentCopyRequest::source_pitch");
    return source_pitch_;
}

uint32_t PresentCopyRequest::destination_entries() const
{
    AV_SCOPE("PresentCopyRequest::destination_entries");
    return destination_entries_;
}

uint32_t PresentCopyRequest::destination_pitch() const
{
    AV_SCOPE("PresentCopyRequest::destination_pitch");
    return destination_pitch_;
}

size_t PresentCopyRequest::destination_entries_offset() const
{
    AV_SCOPE("PresentCopyRequest::destination_entries_offset");
    return encoded_size + size_t{source_entries_} * entry_size;
}

size_t PresentCopyRequest::regions_offset() const
{
    AV_SCOPE("PresentCopyRequest::regions_offset");
    return destination_entries_offset() + size_t{destination_entries_} * entry_size;
}

size_t PresentCopyRequest::size() const
{
    AV_SCOPE("PresentCopyRequest::size");
    return regions_offset() + size_t{regions_} * PresentRegion::encoded_size;
}

}  // namespace av
