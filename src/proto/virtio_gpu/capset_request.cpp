#include "proto/virtio_gpu/capset_request.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

CapsetRequest::CapsetRequest() : index_(0u), capset_id_(0u), capset_version_(0u) {}

void CapsetRequest::skip_header(WireReader &reader)
{
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
}

Status CapsetRequest::decode_info(WireReader &reader)
{
    if (reader.remaining() < encoded_size) return Status::malformed;
    skip_header(reader);
    index_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
    return Status::ok;
}

Status CapsetRequest::decode_capset(WireReader &reader)
{
    if (reader.remaining() < encoded_size) return Status::malformed;
    skip_header(reader);
    capset_id_ = reader.read_u32();
    capset_version_ = reader.read_u32();
    return Status::ok;
}

uint32_t CapsetRequest::index() const { return index_; }
uint32_t CapsetRequest::capset_id() const { return capset_id_; }
uint32_t CapsetRequest::capset_version() const { return capset_version_; }

}  // namespace av
