#include "proto/renderer/reply_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ReplyMessage::ReplyMessage() : sequence_(0u), status_(0u), payload_(nullptr), payload_length_(0u)
{
    AV_SCOPE("ReplyMessage::ReplyMessage");
}

ReplyMessage::ReplyMessage(uint64_t sequence, Status status, const uint8_t *payload, uint32_t payload_length)
    : sequence_(sequence), status_(static_cast<uint32_t>(status)), payload_(payload),
      payload_length_(payload != nullptr ? payload_length : 0u)
{
    AV_SCOPE("ReplyMessage::ReplyMessage");
}

uint64_t ReplyMessage::sequence() const
{
    AV_SCOPE("ReplyMessage::sequence");
    return sequence_;
}

Status ReplyMessage::status() const
{
    AV_SCOPE("ReplyMessage::status");
    return static_cast<Status>(status_);
}

const uint8_t *ReplyMessage::payload() const
{
    AV_SCOPE("ReplyMessage::payload");
    return payload_;
}

uint32_t ReplyMessage::payload_length() const
{
    AV_SCOPE("ReplyMessage::payload_length");
    return payload_length_;
}

size_t ReplyMessage::encoded_length() const
{
    AV_SCOPE("ReplyMessage::encoded_length");
    return fixed_size + payload_length_;
}

bool ReplyMessage::fields_valid() const
{
    AV_SCOPE("ReplyMessage::fields_valid");
    return status_ <= static_cast<uint32_t>(Status::malformed) && payload_length_ <= max_payload_length;
}

Status ReplyMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ReplyMessage::encode");
    if (capacity < encoded_length()) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u64(sequence_);
    writer.write_u32(status_);
    writer.write_u32(payload_length_);
    writer.copy_bytes(payload_, payload_length_);
    return writer.status();
}

Status ReplyMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ReplyMessage::decode");
    if (descriptor_count != descriptors) return Status::malformed;
    WireReader   reader(bytes, length);
    ReplyMessage candidate;
    candidate.sequence_ = reader.read_u64();
    candidate.status_ = reader.read_u32();
    candidate.payload_length_ = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (candidate.payload_length_ > max_payload_length || length != candidate.encoded_length())
        return Status::malformed;
    candidate.payload_ = bytes + fixed_size;
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
