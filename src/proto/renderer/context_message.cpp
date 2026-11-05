#include "proto/renderer/context_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextMessage::ContextMessage() : context_(0u), name_{}
{
    AV_SCOPE("ContextMessage::ContextMessage");
}

ContextMessage::ContextMessage(uint32_t context) : context_(context), name_{}
{
    AV_SCOPE("ContextMessage::ContextMessage");
}

ContextMessage::ContextMessage(uint32_t context, const char *name) : context_(context), name_{}
{
    AV_SCOPE("ContextMessage::ContextMessage");
    for (size_t at = 0u; name != nullptr && name[at] != '\0' && at + 1u < name_bytes; at++) name_[at] = name[at];
}

uint32_t ContextMessage::context() const
{
    AV_SCOPE("ContextMessage::context");
    return context_;
}

const char *ContextMessage::name() const
{
    AV_SCOPE("ContextMessage::name");
    return name_;
}

Status ContextMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ContextMessage::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (context_ == 0u) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(context_);
    writer.write(name_, name_bytes);
    return writer.status();
}

Status ContextMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ContextMessage::decode");
    if (length != encoded_size || descriptor_count != descriptors) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t context = reader.read_u32();
    char           name[name_bytes] = {};
    reader.copy_bytes(reinterpret_cast<uint8_t *>(name), name_bytes);
    if (reader.status() != Status::ok) return reader.status();
    if (context == 0u || name[name_bytes - 1u] != '\0') return Status::malformed;
    context_ = context;
    __builtin_memcpy(name_, name, name_bytes);
    return Status::ok;
}

}
