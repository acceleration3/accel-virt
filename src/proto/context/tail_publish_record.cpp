#include "proto/context/tail_publish_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

TailPublishRecord::TailPublishRecord()
{
    AV_SCOPE("TailPublishRecord::TailPublishRecord");
}

Status TailPublishRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("TailPublishRecord::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_zeros(encoded_size - sizeof(uint32_t));
    return writer.status();
}

Status TailPublishRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("TailPublishRecord::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    WireReader reader(bytes, length);
    if (reader.read_u32() != magic) return Status::malformed;
    reader.expect_zero(encoded_size - sizeof(uint32_t));
    return reader.status();
}

}  // namespace av
