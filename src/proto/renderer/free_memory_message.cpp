#include "proto/renderer/free_memory_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

FreeMemoryMessage::FreeMemoryMessage() : key_(0u)
{
    AV_SCOPE("FreeMemoryMessage::FreeMemoryMessage");
}

FreeMemoryMessage::FreeMemoryMessage(uint64_t key) : key_(key)
{
    AV_SCOPE("FreeMemoryMessage::FreeMemoryMessage");
}

uint64_t FreeMemoryMessage::key() const
{
    AV_SCOPE("FreeMemoryMessage::key");
    return key_;
}

Status FreeMemoryMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("FreeMemoryMessage::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    if (key_ == 0u) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u64(key_);
    return writer.status();
}

Status FreeMemoryMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("FreeMemoryMessage::decode");
    if (bytes == nullptr || length != encoded_size || descriptor_count != 0u) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint64_t key = reader.read_u64();
    if (key == 0u) return Status::malformed;
    key_ = key;
    return Status::ok;
}

}  // namespace av
