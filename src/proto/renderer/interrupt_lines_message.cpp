#include "proto/renderer/interrupt_lines_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

InterruptLinesMessage::InterruptLinesMessage() : lines_(0u), present_(0u)
{
    AV_SCOPE("InterruptLinesMessage::InterruptLinesMessage");
}

InterruptLinesMessage::InterruptLinesMessage(uint32_t lines, uint64_t present) : lines_(lines), present_(present)
{
    AV_SCOPE("InterruptLinesMessage::InterruptLinesMessage");
}

uint32_t InterruptLinesMessage::lines() const
{
    AV_SCOPE("InterruptLinesMessage::lines");
    return lines_;
}

bool InterruptLinesMessage::present(uint32_t line) const
{
    AV_SCOPE("InterruptLinesMessage::present");
    return line < lines_ && ((present_ >> line) & 1u) != 0u;
}

uint32_t InterruptLinesMessage::descriptors() const
{
    AV_SCOPE("InterruptLinesMessage::descriptors");
    return static_cast<uint32_t>(__builtin_popcountll(present_));
}

bool InterruptLinesMessage::fields_valid() const
{
    AV_SCOPE("InterruptLinesMessage::fields_valid");
    return lines_ != 0u && lines_ <= max_lines && (lines_ == max_lines || (present_ >> lines_) == 0u);
}

Status InterruptLinesMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("InterruptLinesMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(lines_);
    writer.write_u32(0u);
    writer.write_u64(present_);
    return writer.status();
}

Status InterruptLinesMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("InterruptLinesMessage::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader            reader(bytes, length);
    InterruptLinesMessage candidate;
    candidate.lines_ = reader.read_u32();
    reader.expect_zero(sizeof(uint32_t));
    candidate.present_ = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (!candidate.fields_valid() || descriptor_count != candidate.descriptors()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
