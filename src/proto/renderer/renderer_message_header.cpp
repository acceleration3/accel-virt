#include "proto/renderer/renderer_message_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RendererMessageHeader::RendererMessageHeader() : kind_(0u), body_length_(0u), sequence_(0u)
{
    AV_SCOPE("RendererMessageHeader::RendererMessageHeader");
}

RendererMessageHeader::RendererMessageHeader(Kind kind, uint32_t body_length, uint64_t sequence)
    : kind_(static_cast<uint32_t>(kind)), body_length_(body_length), sequence_(sequence)
{
    AV_SCOPE("RendererMessageHeader::RendererMessageHeader");
}

RendererMessageHeader::Kind RendererMessageHeader::kind() const
{
    AV_SCOPE("RendererMessageHeader::kind");
    return static_cast<Kind>(kind_);
}

uint32_t RendererMessageHeader::body_length() const
{
    AV_SCOPE("RendererMessageHeader::body_length");
    return body_length_;
}

uint64_t RendererMessageHeader::sequence() const
{
    AV_SCOPE("RendererMessageHeader::sequence");
    return sequence_;
}

bool RendererMessageHeader::fields_valid() const
{
    AV_SCOPE("RendererMessageHeader::fields_valid");
    const bool known = kind_ >= static_cast<uint32_t>(Kind::hello) && kind_ <= static_cast<uint32_t>(Kind::exported_image);
    const bool empty_when_off = kind_ != static_cast<uint32_t>(Kind::interrupts_off) || body_length_ == 0u;
    return known && body_length_ <= max_body_length && empty_when_off;
}

Status RendererMessageHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("RendererMessageHeader::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(kind_);
    writer.write_u32(body_length_);
    writer.write_u64(sequence_);
    return writer.status();
}

Status RendererMessageHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("RendererMessageHeader::decode");
    WireReader            reader(bytes, length);
    RendererMessageHeader candidate;
    candidate.kind_ = reader.read_u32();
    candidate.body_length_ = reader.read_u32();
    candidate.sequence_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (!candidate.fields_valid() || reader.remaining() != candidate.body_length_) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
