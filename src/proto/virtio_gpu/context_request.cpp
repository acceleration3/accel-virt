#include "proto/virtio_gpu/context_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

ContextRequest::ContextRequest() : context_id_(0u), name_length_(0u), context_init_(0u), command_size_(0u), name_{}
{
    AV_SCOPE("ContextRequest::ContextRequest");
}

void ContextRequest::read_header(WireReader &reader)
{
    AV_SCOPE("ContextRequest::read_header");
    reader.skip(context_id_offset);
    context_id_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
}

Status ContextRequest::decode_destroy(WireReader &reader)
{
    AV_SCOPE("ContextRequest::decode_destroy");
    if (reader.remaining() < header_size) return Status::malformed;
    read_header(reader);
    return Status::ok;
}

Status ContextRequest::decode_create(WireReader &reader)
{
    AV_SCOPE("ContextRequest::decode_create");
    if (reader.remaining() < create_size) return Status::malformed;
    read_header(reader);
    name_length_ = reader.read_u32();
    context_init_ = reader.read_u32();
    uint8_t raw[name_capacity] = {};
    reader.copy_bytes(raw, sizeof raw);
    const uint32_t kept = name_length_ < name_capacity ? name_length_ : name_capacity;
    uint32_t       length = 0u;
    while (length < kept && raw[length] != 0u) {
        name_[length] = raw[length] >= 0x20u && raw[length] < 0x7Fu ? static_cast<char>(raw[length]) : '?';
        length++;
    }
    name_[length] = '\0';
    return Status::ok;
}

Status ContextRequest::decode_submit(WireReader &reader)
{
    AV_SCOPE("ContextRequest::decode_submit");
    if (reader.remaining() < submit_size) return Status::malformed;
    read_header(reader);
    command_size_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
    return Status::ok;
}

uint32_t ContextRequest::context_id() const
{
    AV_SCOPE("ContextRequest::context_id");
    return context_id_;
}
uint32_t ContextRequest::name_length() const
{
    AV_SCOPE("ContextRequest::name_length");
    return name_length_;
}
uint32_t ContextRequest::context_init() const
{
    AV_SCOPE("ContextRequest::context_init");
    return context_init_;
}
uint32_t ContextRequest::command_size() const
{
    AV_SCOPE("ContextRequest::command_size");
    return command_size_;
}
const char *ContextRequest::name() const
{
    AV_SCOPE("ContextRequest::name");
    return name_;
}

}
