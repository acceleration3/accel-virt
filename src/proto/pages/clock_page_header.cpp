#include "proto/pages/clock_page_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ClockPageHeader::ClockPageHeader() : heads_(0u)
{
    AV_SCOPE("ClockPageHeader::ClockPageHeader");
}

ClockPageHeader::ClockPageHeader(uint32_t heads) : heads_(heads)
{
    AV_SCOPE("ClockPageHeader::ClockPageHeader");
}

uint32_t ClockPageHeader::heads() const
{
    AV_SCOPE("ClockPageHeader::heads");
    return heads_;
}

size_t ClockPageHeader::record_offset(uint32_t head) const
{
    AV_SCOPE("ClockPageHeader::record_offset");
    return encoded_size + static_cast<size_t>(head) * record_size;
}

Status ClockPageHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ClockPageHeader::encode");
    if (heads_ == 0u || heads_ > max_heads) return Status::invalid_parameter;
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(version);
    writer.write_u32(heads_);
    writer.write_zeros(reserved_bytes);
    return writer.status();
}

Status ClockPageHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ClockPageHeader::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t found_version = reader.read_u32();
    const uint32_t heads = reader.read_u32();
    reader.expect_zero(reserved_bytes);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version) return Status::malformed;
    if (heads == 0u || heads > max_heads) return Status::malformed;
    heads_ = heads;
    return Status::ok;
}

}  // namespace av
