#include "proto/escape/context_views_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextViewsRecord::ContextViewsRecord() : ring_(0u), reply_(0u)
{
    AV_SCOPE("ContextViewsRecord::ContextViewsRecord");
}

uint64_t ContextViewsRecord::ring() const
{
    AV_SCOPE("ContextViewsRecord::ring");
    return ring_;
}

uint64_t ContextViewsRecord::reply() const
{
    AV_SCOPE("ContextViewsRecord::reply");
    return reply_;
}

ContextViewsRecord ContextViewsRecord::answered(uint64_t ring, uint64_t reply) const
{
    AV_SCOPE("ContextViewsRecord::answered");
    ContextViewsRecord answer = *this;
    answer.ring_ = ring;
    answer.reply_ = reply;
    return answer;
}

Status ContextViewsRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ContextViewsRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_zeros(2u);
    writer.write_u64(ring_);
    writer.write_u64(reply_);
    writer.write_zeros(8u);
    return writer.status();
}

Status ContextViewsRecord::read(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::read");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    reader.expect_zero(2u);
    ring_ = reader.read_u64();
    reply_ = reader.read_u64();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    return found_magic == magic && found_version == version ? Status::ok : Status::malformed;
}

Status ContextViewsRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::decode");
    ContextViewsRecord candidate;
    const Status       read_back = candidate.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (candidate.ring_ != 0u || candidate.reply_ != 0u) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

Status ContextViewsRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::accept_answer");
    ContextViewsRecord answer;
    const Status       read_back = answer.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (answer.ring_ == 0u || answer.reply_ == 0u || answer.ring_ == answer.reply_) return Status::malformed;
    *this = answer;
    return Status::ok;
}

}  // namespace av
