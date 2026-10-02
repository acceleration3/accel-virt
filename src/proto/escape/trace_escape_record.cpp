#include "proto/escape/trace_escape_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

TraceEscapeRecord::TraceEscapeRecord() : kind_(Kind::table), offset_(0u), window_(0u), total_(0u), clock_(0u)
{
    AV_SCOPE("TraceEscapeRecord::TraceEscapeRecord");
}

TraceEscapeRecord::TraceEscapeRecord(Kind kind, uint64_t offset, uint32_t window)
    : kind_(kind), offset_(offset), window_(window), total_(0u), clock_(0u)
{
    AV_SCOPE("TraceEscapeRecord::TraceEscapeRecord");
}

TraceEscapeRecord::Kind TraceEscapeRecord::kind() const
{
    AV_SCOPE("TraceEscapeRecord::kind");
    return kind_;
}
uint64_t                TraceEscapeRecord::offset() const
{
    AV_SCOPE("TraceEscapeRecord::offset");
    return offset_;
}
uint32_t                TraceEscapeRecord::window() const
{
    AV_SCOPE("TraceEscapeRecord::window");
    return window_;
}
uint64_t                TraceEscapeRecord::total() const
{
    AV_SCOPE("TraceEscapeRecord::total");
    return total_;
}
uint64_t                TraceEscapeRecord::clock() const
{
    AV_SCOPE("TraceEscapeRecord::clock");
    return clock_;
}

TraceEscapeRecord TraceEscapeRecord::answered(uint64_t total, uint32_t filled, uint64_t clock) const
{
    AV_SCOPE("TraceEscapeRecord::answered");
    TraceEscapeRecord answer = *this;
    answer.total_ = total;
    answer.window_ = filled;
    answer.clock_ = clock;
    return answer;
}

Status TraceEscapeRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("TraceEscapeRecord::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_u16(static_cast<uint16_t>(kind_));
    writer.write_u64(offset_);
    writer.write_u64(total_);
    writer.write_u64(clock_);
    writer.write_u32(window_);
    writer.write_zeros(4u);
    return writer.status();
}

Status TraceEscapeRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("TraceEscapeRecord::decode");
    TraceEscapeRecord request;
    if (request.read(bytes, length) != Status::ok) return Status::malformed;
    if (request.total_ != 0u || request.clock_ != 0u) return Status::malformed;
    if (request.window_ > length - encoded_size) return Status::malformed;
    *this = request;
    return Status::ok;
}

Status TraceEscapeRecord::accept_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("TraceEscapeRecord::accept_answer");
    TraceEscapeRecord answer;
    if (answer.read(bytes, length) != Status::ok) return Status::malformed;
    if (answer.kind_ != kind_ || answer.offset_ != offset_ || answer.window_ > window_) return Status::malformed;
    if (answer.window_ > length - encoded_size) return Status::malformed;
    if (answer.offset_ > answer.total_ || answer.window_ > answer.total_ - answer.offset_) return Status::malformed;
    *this = answer;
    return Status::ok;
}

Status TraceEscapeRecord::read(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("TraceEscapeRecord::read");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    const uint16_t kind = reader.read_u16();
    offset_ = reader.read_u64();
    total_ = reader.read_u64();
    clock_ = reader.read_u64();
    window_ = reader.read_u32();
    reader.expect_zero(4u);
    if (reader.status() != Status::ok) return Status::malformed;
    if (found_magic != magic || found_version != version) return Status::malformed;
    if (kind != static_cast<uint16_t>(Kind::table) && kind != static_cast<uint16_t>(Kind::state))
        return Status::malformed;
    kind_ = static_cast<Kind>(kind);
    return Status::ok;
}

}  // namespace av
