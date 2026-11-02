#include "proto/renderer/region_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RegionMessage::RegionMessage() : region_(0u), size_(0u)
{
    AV_SCOPE("RegionMessage::RegionMessage");
}

RegionMessage::RegionMessage(uint32_t region, uint64_t size) : region_(region), size_(size)
{
    AV_SCOPE("RegionMessage::RegionMessage");
}

uint32_t RegionMessage::region() const
{
    AV_SCOPE("RegionMessage::region");
    return region_;
}

uint64_t RegionMessage::size() const
{
    AV_SCOPE("RegionMessage::size");
    return size_;
}

bool RegionMessage::fields_valid() const
{
    AV_SCOPE("RegionMessage::fields_valid");
    return region_ < RendererExtent::region_count && size_ != 0u && size_ <= RendererExtent::region_span;
}

Status RegionMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("RegionMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(region_);
    writer.write_zeros(sizeof(uint32_t));
    writer.write_u64(size_);
    return writer.status();
}

Status RegionMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("RegionMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader    reader(bytes, length);
    RegionMessage candidate;
    candidate.region_ = reader.read_u32();
    reader.expect_zero(sizeof(uint32_t));
    candidate.size_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
