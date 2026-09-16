#include "proto/submission/command_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

CommandHeader::CommandHeader() : id_(0u), size_(0u), flags_(0u)
{
    AV_SCOPE("CommandHeader::CommandHeader");
}

CommandHeader::CommandHeader(uint32_t id, uint32_t size, uint8_t flags) : id_(id), size_(size), flags_(flags)
{
    AV_SCOPE("CommandHeader::CommandHeader");
}

uint32_t CommandHeader::id() const
{
    AV_SCOPE("CommandHeader::id");
    return id_;
}
uint16_t CommandHeader::api() const
{
    AV_SCOPE("CommandHeader::api");
    return static_cast<uint16_t>(id_ >> 16u);
}
uint32_t CommandHeader::size() const
{
    AV_SCOPE("CommandHeader::size");
    return size_;
}
bool     CommandHeader::generates_reply() const
{
    AV_SCOPE("CommandHeader::generates_reply");
    return (flags_ & generate_reply) != 0u;
}

Status CommandHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("CommandHeader::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(id_);
    writer.write_u32((size_ & max_size) | (uint32_t{flags_} << flags_shift));
    return writer.status();
}

Status CommandHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("CommandHeader::decode");
    WireReader     reader(bytes, length);
    const uint32_t id = reader.read_u32();
    const uint32_t word = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    const uint32_t size = word & max_size;
    const uint8_t  flags = static_cast<uint8_t>(word >> flags_shift);
    if (size < encoded_size || (flags & ~generate_reply) != 0u) return Status::malformed;
    *this = CommandHeader(id, size, flags);
    return Status::ok;
}

}  // namespace av
