#include "proto/renderer/ready_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ReadyMessage::ReadyMessage() : version_(0u), executor_abi_(0u), sub_apis_(0u)
{
    AV_SCOPE("ReadyMessage::ReadyMessage");
}

ReadyMessage::ReadyMessage(uint32_t version, uint32_t executor_abi, uint32_t sub_apis)
    : version_(version), executor_abi_(executor_abi), sub_apis_(sub_apis)
{
    AV_SCOPE("ReadyMessage::ReadyMessage");
}

uint32_t ReadyMessage::version() const
{
    AV_SCOPE("ReadyMessage::version");
    return version_;
}

uint32_t ReadyMessage::executor_abi() const
{
    AV_SCOPE("ReadyMessage::executor_abi");
    return executor_abi_;
}

uint32_t ReadyMessage::sub_apis() const
{
    AV_SCOPE("ReadyMessage::sub_apis");
    return sub_apis_;
}

bool ReadyMessage::fields_valid() const
{
    AV_SCOPE("ReadyMessage::fields_valid");
    return (sub_apis_ >> lanes) == 0u;
}

Status ReadyMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ReadyMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(version_);
    writer.write_u32(executor_abi_);
    writer.write_u32(sub_apis_);
    return writer.status();
}

Status ReadyMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ReadyMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader   reader(bytes, length);
    ReadyMessage candidate;
    candidate.version_ = reader.read_u32();
    candidate.executor_abi_ = reader.read_u32();
    candidate.sub_apis_ = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
