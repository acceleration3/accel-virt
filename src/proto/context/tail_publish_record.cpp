#include "proto/context/tail_publish_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

TailPublishRecord::TailPublishRecord() : ring_(0u)
{
    AV_SCOPE("TailPublishRecord::TailPublishRecord");
}

TailPublishRecord::TailPublishRecord(uint32_t ring) : ring_(ring)
{
    AV_SCOPE("TailPublishRecord::TailPublishRecord");
}

uint32_t TailPublishRecord::ring() const
{
    AV_SCOPE("TailPublishRecord::ring");
    return ring_;
}

Status TailPublishRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("TailPublishRecord::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(ring_);
    writer.write_zeros(encoded_size - 2u * sizeof(uint32_t));
    return writer.status();
}

Status TailPublishRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("TailPublishRecord::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    WireReader reader(bytes, length);
    if (reader.read_u32() != magic) return Status::malformed;
    const uint32_t ring = reader.read_u32();
    reader.expect_zero(encoded_size - 2u * sizeof(uint32_t));
    if (reader.status() != Status::ok) return reader.status();
    ring_ = ring;
    return Status::ok;
}

}  // namespace av
