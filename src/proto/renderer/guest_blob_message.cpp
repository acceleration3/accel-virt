#include "proto/renderer/guest_blob_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/renderer/renderer_message_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

static_assert(GuestBlobMessage::fixed_size + size_t{GuestBlobMessage::max_extents} * RendererExtent::encoded_size <=
                  RendererMessageHeader::max_body_length,
              "the largest guest blob is one datagram");

GuestBlobMessage::GuestBlobMessage() : blob_(0u), size_(0u), extents_(nullptr), count_(0u)
{
    AV_SCOPE("GuestBlobMessage::GuestBlobMessage");
}

GuestBlobMessage::GuestBlobMessage(uint32_t blob, uint64_t size, const RendererExtent *extents, uint32_t count)
    : blob_(blob), size_(size), extents_(extents), count_(extents != nullptr ? count : 0u)
{
    AV_SCOPE("GuestBlobMessage::GuestBlobMessage");
}

uint32_t GuestBlobMessage::blob() const
{
    AV_SCOPE("GuestBlobMessage::blob");
    return blob_;
}

uint64_t GuestBlobMessage::size() const
{
    AV_SCOPE("GuestBlobMessage::size");
    return size_;
}

uint32_t GuestBlobMessage::count() const
{
    AV_SCOPE("GuestBlobMessage::count");
    return count_;
}

RendererExtent GuestBlobMessage::extent(uint32_t index) const
{
    AV_SCOPE("GuestBlobMessage::extent");
    return index < count_ ? extents_[index] : RendererExtent();
}

size_t GuestBlobMessage::encoded_length() const
{
    AV_SCOPE("GuestBlobMessage::encoded_length");
    return fixed_size + size_t{count_} * RendererExtent::encoded_size;
}

bool GuestBlobMessage::extents_cover_size() const
{
    AV_SCOPE("GuestBlobMessage::extents_cover_size");
    uint64_t covered = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        const RendererExtent &extent = extents_[index];
        if (!extent.fits() || extent.length() > size_ - covered) return false;
        covered += extent.length();
    }
    return covered == size_;
}

bool GuestBlobMessage::fields_valid() const
{
    AV_SCOPE("GuestBlobMessage::fields_valid");
    return blob_ != 0u && size_ != 0u && count_ != 0u && count_ <= max_extents && extents_cover_size();
}

Status GuestBlobMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("GuestBlobMessage::encode");
    if (capacity < encoded_length()) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(blob_);
    writer.write_u32(count_);
    writer.write_u64(size_);
    for (uint32_t index = 0u; index < count_; index++) extents_[index].write(writer);
    return writer.status();
}

Status GuestBlobMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count,
                                RendererExtent *extents, uint32_t capacity)
{
    AV_SCOPE("GuestBlobMessage::decode");
    if (descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t blob = reader.read_u32();
    const uint32_t count = reader.read_u32();
    const uint64_t size = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (count > max_extents || length != fixed_size + size_t{count} * RendererExtent::encoded_size)
        return Status::malformed;
    const uint32_t room = extents != nullptr ? capacity : 0u;
    if (count > room) return Status::no_space;
    for (uint32_t index = 0u; index < count; index++) extents[index].read(reader);
    const GuestBlobMessage candidate(blob, size, extents, count);
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
