#include "proto/virtio_gpu/capset_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

CapsetRequest::CapsetRequest() : index_(0u), capset_id_(0u), capset_version_(0u)
{
    AV_SCOPE("CapsetRequest::CapsetRequest");
}

void CapsetRequest::skip_header(WireReader &reader)
{
    AV_SCOPE("CapsetRequest::skip_header");
    reader.skip(header_size);
}

Status CapsetRequest::decode_info(WireReader &reader)
{
    AV_SCOPE("CapsetRequest::decode_info");
    if (reader.remaining() < encoded_size) return Status::malformed;
    skip_header(reader);
    index_ = reader.read_u32();
    static_cast<void>(reader.read_u32());
    return Status::ok;
}

Status CapsetRequest::decode_capset(WireReader &reader)
{
    AV_SCOPE("CapsetRequest::decode_capset");
    if (reader.remaining() < encoded_size) return Status::malformed;
    skip_header(reader);
    capset_id_ = reader.read_u32();
    capset_version_ = reader.read_u32();
    return Status::ok;
}

uint32_t CapsetRequest::index() const
{
    AV_SCOPE("CapsetRequest::index");
    return index_;
}
uint32_t CapsetRequest::capset_id() const
{
    AV_SCOPE("CapsetRequest::capset_id");
    return capset_id_;
}
uint32_t CapsetRequest::capset_version() const
{
    AV_SCOPE("CapsetRequest::capset_version");
    return capset_version_;
}

}
