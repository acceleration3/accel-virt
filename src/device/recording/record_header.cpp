#include "device/recording/record_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RecordHeader::RecordHeader()
    : kind_(RecordKind::context_setup), context_(0u), length_(0u), timestamp_(0u), fence_(0u)
{
    AV_SCOPE("RecordHeader::RecordHeader");
}

RecordHeader::RecordHeader(RecordKind kind, uint32_t context, uint32_t length, uint64_t timestamp,
                           uint64_t fence)
    : kind_(kind), context_(context), length_(length), timestamp_(timestamp), fence_(fence)
{
    AV_SCOPE("RecordHeader::RecordHeader");
}

RecordKind RecordHeader::kind() const
{
    AV_SCOPE("RecordHeader::kind");
    return kind_;
}

uint32_t RecordHeader::context() const
{
    AV_SCOPE("RecordHeader::context");
    return context_;
}

uint32_t RecordHeader::length() const
{
    AV_SCOPE("RecordHeader::length");
    return length_;
}

uint64_t RecordHeader::timestamp() const
{
    AV_SCOPE("RecordHeader::timestamp");
    return timestamp_;
}

uint64_t RecordHeader::fence() const
{
    AV_SCOPE("RecordHeader::fence");
    return fence_;
}

uint64_t RecordHeader::span() const
{
    AV_SCOPE("RecordHeader::span");
    return encoded_size + uint64_t{length_};
}

bool RecordHeader::fits(uint64_t remaining) const
{
    AV_SCOPE("RecordHeader::fits");
    return span() <= remaining;
}

Status RecordHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("RecordHeader::encode");
    if (length_ > max_payload) return Status::invalid_parameter;
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(static_cast<uint16_t>(kind_));
    writer.write_u16(0u);
    writer.write_u32(context_);
    writer.write_u32(length_);
    writer.write_u64(timestamp_);
    writer.write_u64(fence_);
    return writer.status();
}

Status RecordHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("RecordHeader::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t kind = reader.read_u16();
    reader.expect_zero(2u);
    const uint32_t context = reader.read_u32();
    const uint32_t payload = reader.read_u32();
    const uint64_t timestamp = reader.read_u64();
    const uint64_t fence = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic) return Status::malformed;
    if (kind < lowest_kind || kind > highest_kind) return Status::malformed;
    if (payload > max_payload) return Status::malformed;
    *this = RecordHeader(static_cast<RecordKind>(kind), context, payload, timestamp, fence);
    return Status::ok;
}

}
