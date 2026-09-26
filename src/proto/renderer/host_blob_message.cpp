#include "proto/renderer/host_blob_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

HostBlobMessage::HostBlobMessage() : blob_(0u), size_(0u)
{
    AV_SCOPE("HostBlobMessage::HostBlobMessage");
}

HostBlobMessage::HostBlobMessage(uint32_t blob, uint64_t size) : blob_(blob), size_(size)
{
    AV_SCOPE("HostBlobMessage::HostBlobMessage");
}

uint32_t HostBlobMessage::blob() const
{
    AV_SCOPE("HostBlobMessage::blob");
    return blob_;
}

uint64_t HostBlobMessage::size() const
{
    AV_SCOPE("HostBlobMessage::size");
    return size_;
}

Status HostBlobMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("HostBlobMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (blob_ == 0u || size_ == 0u) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(blob_);
    writer.write_zeros(sizeof(uint32_t));
    writer.write_u64(size_);
    return writer.status();
}

Status HostBlobMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("HostBlobMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t blob = reader.read_u32();
    reader.expect_zero(sizeof(uint32_t));
    const uint64_t size = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (blob == 0u || size == 0u) return Status::malformed;
    *this = HostBlobMessage(blob, size);
    return Status::ok;
}

}  // namespace av
