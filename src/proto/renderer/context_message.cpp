#include "proto/renderer/context_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextMessage::ContextMessage() : context_(0u)
{
    AV_SCOPE("ContextMessage::ContextMessage");
}

ContextMessage::ContextMessage(uint32_t context) : context_(context)
{
    AV_SCOPE("ContextMessage::ContextMessage");
}

uint32_t ContextMessage::context() const
{
    AV_SCOPE("ContextMessage::context");
    return context_;
}

Status ContextMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ContextMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (context_ == 0u) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(context_);
    return writer.status();
}

Status ContextMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ContextMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t context = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (context == 0u) return Status::malformed;
    context_ = context;
    return Status::ok;
}

}  // namespace av
