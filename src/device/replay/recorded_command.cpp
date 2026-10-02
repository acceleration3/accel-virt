#include "device/replay/recorded_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/command_api.hpp"


namespace av {

RecordedCommand::RecordedCommand() : header_(), payload_(nullptr), payload_length_(0u), offset_(0u)
{
    AV_SCOPE("RecordedCommand::RecordedCommand");
}

Status RecordedCommand::decode(const uint8_t *bytes, uint32_t length, uint32_t offset)
{
    AV_SCOPE("RecordedCommand::decode");
    if (bytes == nullptr || offset > length) return Status::malformed;
    CommandHeader  framed;
    const uint32_t left = length - offset;
    const Status   decoded = framed.decode(bytes + offset, left);
    if (decoded != Status::ok) return decoded;
    if (framed.size() > left) return Status::malformed;
    header_ = framed;
    payload_ = bytes + offset + CommandHeader::encoded_size;
    payload_length_ = framed.size() - CommandHeader::encoded_size;
    offset_ = offset;
    return Status::ok;
}

const CommandHeader &RecordedCommand::header() const
{
    AV_SCOPE("RecordedCommand::header");
    return header_;
}

const uint8_t *RecordedCommand::payload() const
{
    AV_SCOPE("RecordedCommand::payload");
    return payload_;
}

uint32_t RecordedCommand::payload_length() const
{
    AV_SCOPE("RecordedCommand::payload_length");
    return payload_length_;
}

uint32_t RecordedCommand::offset() const
{
    AV_SCOPE("RecordedCommand::offset");
    return offset_;
}

uint32_t RecordedCommand::next_offset() const
{
    AV_SCOPE("RecordedCommand::next_offset");
    return offset_ + header_.size();
}

/* The names come from the tables the generator writes from the registries
 * (FR-005); the transport's three are this repository's own (CommandApi). */
const char *RecordedCommand::name() const
{
    AV_SCOPE("RecordedCommand::name");
    return CommandApi(header_.api()).command_name(header_.id());
}

const char *RecordedCommand::api_name() const
{
    AV_SCOPE("RecordedCommand::api_name");
    return CommandApi(header_.api()).name();
}

}  // namespace av
