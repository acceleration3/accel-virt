#include "proto/renderer/hello_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

HelloMessage::HelloMessage()
    : version_(0u), epoch_(0u), gpu_uuid_(), executor_path_(nullptr), path_length_(0u), capset_(nullptr),
      capset_length_(0u)
{
    AV_SCOPE("HelloMessage::HelloMessage");
}

HelloMessage::HelloMessage(uint32_t version, uint32_t epoch, const DeviceUuid &gpu_uuid,
                           const char *executor_path, uint32_t path_length, const uint8_t *capset,
                           uint32_t capset_length)
    : version_(version), epoch_(epoch), gpu_uuid_(gpu_uuid), executor_path_(executor_path),
      path_length_(executor_path != nullptr ? path_length : 0u), capset_(capset),
      capset_length_(capset != nullptr ? capset_length : 0u)
{
    AV_SCOPE("HelloMessage::HelloMessage");
}

uint32_t HelloMessage::version() const
{
    AV_SCOPE("HelloMessage::version");
    return version_;
}

uint32_t HelloMessage::epoch() const
{
    AV_SCOPE("HelloMessage::epoch");
    return epoch_;
}

const uint8_t *HelloMessage::gpu_uuid() const
{
    AV_SCOPE("HelloMessage::gpu_uuid");
    return gpu_uuid_.raw();
}

const char *HelloMessage::executor_path() const
{
    AV_SCOPE("HelloMessage::executor_path");
    return executor_path_;
}

uint32_t HelloMessage::executor_path_length() const
{
    AV_SCOPE("HelloMessage::executor_path_length");
    return path_length_;
}

const uint8_t *HelloMessage::capset() const
{
    AV_SCOPE("HelloMessage::capset");
    return capset_;
}

uint32_t HelloMessage::capset_length() const
{
    AV_SCOPE("HelloMessage::capset_length");
    return capset_length_;
}

size_t HelloMessage::encoded_length() const
{
    AV_SCOPE("HelloMessage::encoded_length");
    return fixed_size + path_length_ + capset_length_;
}

bool HelloMessage::fields_valid() const
{
    AV_SCOPE("HelloMessage::fields_valid");
    if (path_length_ == 0u || path_length_ > max_path_length) return false;
    if (capset_length_ == 0u || capset_length_ > max_capset_length) return false;
    for (uint32_t index = 0u; index < path_length_; index++)
        if (executor_path_[index] == '\0') return false;
    return true;
}

Status HelloMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("HelloMessage::encode");
    if (capacity < encoded_length()) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(version_);
    writer.write_u32(epoch_);
    gpu_uuid_.encode(writer);
    writer.write_u32(path_length_);
    writer.write_u32(capset_length_);
    writer.copy_bytes(reinterpret_cast<const uint8_t *>(executor_path_), path_length_);
    writer.copy_bytes(capset_, capset_length_);
    return writer.status();
}

Status HelloMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("HelloMessage::decode");
    if (descriptor_count != descriptors) return Status::malformed;
    WireReader   reader(bytes, length);
    HelloMessage candidate;
    candidate.version_ = reader.read_u32();
    candidate.epoch_ = reader.read_u32();
    candidate.gpu_uuid_.decode(reader);
    candidate.path_length_ = reader.read_u32();
    candidate.capset_length_ = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    /* Bounded before they are summed, so the sum cannot wrap a 32-bit size_t. */
    if (candidate.path_length_ > max_path_length || candidate.capset_length_ > max_capset_length ||
        length != candidate.encoded_length())
        return Status::malformed;
    candidate.executor_path_ = reinterpret_cast<const char *>(bytes + fixed_size);
    candidate.capset_ = bytes + fixed_size + candidate.path_length_;
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
