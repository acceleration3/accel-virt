#include "proto/escape/context_views_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextViewsRecord::ContextViewsRecord() : version_(with_fence), ring_(0u), reply_(0u), fence_(0u)
{
    AV_SCOPE("ContextViewsRecord::ContextViewsRecord");
}

ContextViewsRecord::ContextViewsRecord(uint16_t version) : version_(version), ring_(0u), reply_(0u), fence_(0u)
{
    AV_SCOPE("ContextViewsRecord::ContextViewsRecord");
}

uint16_t ContextViewsRecord::version() const
{
    AV_SCOPE("ContextViewsRecord::version");
    return version_;
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

uint64_t ContextViewsRecord::fence() const
{
    AV_SCOPE("ContextViewsRecord::fence");
    return fence_;
}

ContextViewsRecord ContextViewsRecord::answered(uint64_t ring, uint64_t reply, uint64_t fence) const
{
    AV_SCOPE("ContextViewsRecord::answered");
    ContextViewsRecord answer = *this;
    answer.ring_ = ring;
    answer.reply_ = reply;
    answer.fence_ = version_ == with_fence ? fence : 0u;
    return answer;
}

Status ContextViewsRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ContextViewsRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version_);
    writer.write_zeros(2u);
    writer.write_u64(ring_);
    writer.write_u64(reply_);
    writer.write_u64(fence_);
    return writer.status();
}

Status ContextViewsRecord::read(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::read");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    version_ = reader.read_u16();
    reader.expect_zero(2u);
    ring_ = reader.read_u64();
    reply_ = reader.read_u64();
    fence_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic) return Status::malformed;
    if (version_ == without_fence) return fence_ == 0u ? Status::ok : Status::malformed;
    return version_ == with_fence ? Status::ok : Status::malformed;
}

Status ContextViewsRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::decode");
    ContextViewsRecord candidate;
    const Status       read_back = candidate.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (candidate.ring_ != 0u || candidate.reply_ != 0u || candidate.fence_ != 0u) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

Status ContextViewsRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextViewsRecord::accept_answer");
    ContextViewsRecord answer;
    const Status       read_back = answer.read(bytes, length);
    if (read_back != Status::ok) return read_back;
    if (answer.version_ != version_) return Status::malformed;
    if (answer.ring_ == 0u || answer.reply_ == 0u || answer.ring_ == answer.reply_) return Status::malformed;
    if (version_ == with_fence &&
        (answer.fence_ == 0u || answer.fence_ == answer.ring_ || answer.fence_ == answer.reply_))
        return Status::malformed;
    *this = answer;
    return Status::ok;
}

}
