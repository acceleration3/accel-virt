#include "proto/virtio_gpu/shared_release_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

SharedReleaseRequest::SharedReleaseRequest() : object_id_(0u)
{
    AV_SCOPE("SharedReleaseRequest::SharedReleaseRequest");
}

Status SharedReleaseRequest::decode(WireReader &reader)
{
    AV_SCOPE("SharedReleaseRequest::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    const uint64_t object_id = reader.read_u64();
    if (object_id == 0u) return Status::malformed;
    object_id_ = object_id;
    return Status::ok;
}

uint64_t SharedReleaseRequest::object_id() const
{
    AV_SCOPE("SharedReleaseRequest::object_id");
    return object_id_;
}

}  // namespace av
