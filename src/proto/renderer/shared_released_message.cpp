#include "proto/renderer/shared_released_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SharedReleasedMessage::SharedReleasedMessage() : object_id_(0u)
{
    AV_SCOPE("SharedReleasedMessage::SharedReleasedMessage");
}

SharedReleasedMessage::SharedReleasedMessage(uint64_t object_id) : object_id_(object_id)
{
    AV_SCOPE("SharedReleasedMessage::SharedReleasedMessage");
}

uint64_t SharedReleasedMessage::object_id() const
{
    AV_SCOPE("SharedReleasedMessage::object_id");
    return object_id_;
}

Status SharedReleasedMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("SharedReleasedMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u64(object_id_);
    return writer.status();
}

Status SharedReleasedMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("SharedReleasedMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint64_t object_id = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    object_id_ = object_id;
    return Status::ok;
}

}
