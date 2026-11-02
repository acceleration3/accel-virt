#include "proto/renderer/export_image_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ExportImageMessage::ExportImageMessage() : key_(0u)
{
    AV_SCOPE("ExportImageMessage::ExportImageMessage");
}

ExportImageMessage::ExportImageMessage(uint64_t key) : key_(key)
{
    AV_SCOPE("ExportImageMessage::ExportImageMessage");
}

uint64_t ExportImageMessage::key() const
{
    AV_SCOPE("ExportImageMessage::key");
    return key_;
}

Status ExportImageMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ExportImageMessage::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u64(key_);
    return writer.status();
}

Status ExportImageMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ExportImageMessage::decode");
    if (bytes == nullptr || length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint64_t key = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    key_ = key;
    return Status::ok;
}

}
