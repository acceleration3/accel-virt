#include "proto/renderer/blob_retired_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobRetiredMessage::BlobRetiredMessage() : blob_(0u)
{
    AV_SCOPE("BlobRetiredMessage::BlobRetiredMessage");
}

BlobRetiredMessage::BlobRetiredMessage(uint32_t blob) : blob_(blob)
{
    AV_SCOPE("BlobRetiredMessage::BlobRetiredMessage");
}

uint32_t BlobRetiredMessage::blob() const
{
    AV_SCOPE("BlobRetiredMessage::blob");
    return blob_;
}

Status BlobRetiredMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BlobRetiredMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (blob_ == 0u) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(blob_);
    return writer.status();
}

Status BlobRetiredMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("BlobRetiredMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t blob = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (blob == 0u) return Status::malformed;
    blob_ = blob;
    return Status::ok;
}

}  // namespace av
