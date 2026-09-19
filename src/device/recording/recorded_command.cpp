#include "device/recording/recorded_command.hpp"

#include "core/trace/function_scope.hpp"

#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/vulkan/vk_command_names.h"

namespace av {

namespace {

constexpr const char *unknown = "unknown";

}  // namespace

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
 * (FR-005); the transport's three are this repository's own, and are the only
 * ids named here. */
const char *RecordedCommand::name() const
{
    AV_SCOPE("RecordedCommand::name");
    switch (header_.api()) {
    case CommandHeader::transport_api:
        if (header_.id() == CommandHeader::nop_id) return "nop";
        if (header_.id() == CommandHeader::import_blob_id) return "import_blob";
        if (header_.id() == CommandHeader::release_blob_id) return "release_blob";
        return unknown;
    case vulkan_api: return vkproto::cmd_id_to_string(header_.id());
    case opencl_api: return clproto::cmd_id_to_string(header_.id());
    case opengl_api: return glproto::cmd_id_to_string(header_.id());
    default: return unknown;
    }
}

const char *RecordedCommand::api_name() const
{
    AV_SCOPE("RecordedCommand::api_name");
    switch (header_.api()) {
    case CommandHeader::transport_api: return "transport";
    case vulkan_api: return "vulkan";
    case opencl_api: return "opencl";
    case opengl_api: return "opengl";
    default: return "api";
    }
}

}  // namespace av
