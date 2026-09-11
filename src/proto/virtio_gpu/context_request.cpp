#include "proto/virtio_gpu/context_request.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

ContextRequest::ContextRequest() : context_id_(0u), name_length_(0u), context_init_(0u), command_size_(0u) {}

void ContextRequest::read_header(WireReader &reader)
{
    for (size_t byte = 0u; byte < context_id_offset; byte++) static_cast<void>(reader.read_u8());
    context_id_ = reader.read_u32();
    static_cast<void>(reader.read_u32());   /* ring_idx and padding */
}

Status ContextRequest::decode_destroy(WireReader &reader)
{
    if (reader.remaining() < header_size) return Status::malformed;
    read_header(reader);
    return Status::ok;
}

Status ContextRequest::decode_create(WireReader &reader)
{
    if (reader.remaining() < create_size) return Status::malformed;
    read_header(reader);
    name_length_ = reader.read_u32();
    context_init_ = reader.read_u32();
    for (size_t byte = 0u; byte < name_capacity; byte++) static_cast<void>(reader.read_u8());
    return Status::ok;
}

Status ContextRequest::decode_submit(WireReader &reader)
{
    if (reader.remaining() < submit_size) return Status::malformed;
    read_header(reader);
    command_size_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
    return Status::ok;
}

uint32_t ContextRequest::context_id() const { return context_id_; }
uint32_t ContextRequest::name_length() const { return name_length_; }
uint32_t ContextRequest::context_init() const { return context_init_; }
uint32_t ContextRequest::command_size() const { return command_size_; }

}  // namespace av
