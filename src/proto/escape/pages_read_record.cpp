#include "proto/escape/pages_read_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PagesReadRecord::PagesReadRecord()
    : action_(Action::begin), allocation_(0u), wait_microseconds_(0u), writes_(0u), complete_(false)
{
    AV_SCOPE("PagesReadRecord::PagesReadRecord");
}

PagesReadRecord::PagesReadRecord(Action action, uint32_t allocation, uint32_t wait_microseconds)
    : action_(action), allocation_(allocation), wait_microseconds_(wait_microseconds), writes_(0u), complete_(false)
{
    AV_SCOPE("PagesReadRecord::PagesReadRecord");
}

PagesReadRecord::Action PagesReadRecord::action() const
{
    AV_SCOPE("PagesReadRecord::action");
    return action_;
}

uint32_t PagesReadRecord::allocation() const
{
    AV_SCOPE("PagesReadRecord::allocation");
    return allocation_;
}

uint32_t PagesReadRecord::wait_microseconds() const
{
    AV_SCOPE("PagesReadRecord::wait_microseconds");
    return wait_microseconds_;
}

uint64_t PagesReadRecord::writes() const
{
    AV_SCOPE("PagesReadRecord::writes");
    return writes_;
}

bool PagesReadRecord::complete() const
{
    AV_SCOPE("PagesReadRecord::complete");
    return complete_;
}

PagesReadRecord PagesReadRecord::answered(uint64_t writes, bool complete) const
{
    AV_SCOPE("PagesReadRecord::answered");
    PagesReadRecord answer = *this;
    answer.writes_ = writes;
    answer.complete_ = complete;
    return answer;
}

Status PagesReadRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("PagesReadRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version);
    writer.write_u16(static_cast<uint16_t>(action_));
    writer.write_u32(allocation_);
    writer.write_u32(wait_microseconds_);
    writer.write_u64(writes_);
    writer.write_u32(complete_ ? 1u : 0u);
    writer.write_zeros(4u);
    return writer.status();
}

Status PagesReadRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PagesReadRecord::decode");
    return read(bytes, length, false);
}

Status PagesReadRecord::read_answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PagesReadRecord::read_answer");
    return read(bytes, length, true);
}

Status PagesReadRecord::read(const uint8_t *bytes, size_t length, bool answered)
{
    AV_SCOPE("PagesReadRecord::read");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_version = reader.read_u16();
    const uint16_t action = reader.read_u16();
    const uint32_t allocation = reader.read_u32();
    const uint32_t wait = reader.read_u32();
    const uint64_t writes = reader.read_u64();
    const uint32_t complete = reader.read_u32();
    reader.expect_zero(4u);
    if (reader.status() != Status::ok) return reader.status();
    const bool action_known = action == static_cast<uint16_t>(Action::begin) ||
                              action == static_cast<uint16_t>(Action::end);
    if (found_magic != magic || found_version != version || !action_known || allocation == 0u) return Status::malformed;
    if (!answered && (writes != 0u || complete != 0u)) return Status::malformed;
    if (complete > 1u) return Status::malformed;
    action_ = static_cast<Action>(action);
    allocation_ = allocation;
    wait_microseconds_ = wait;
    writes_ = writes;
    complete_ = complete == 1u;
    return Status::ok;
}

}
