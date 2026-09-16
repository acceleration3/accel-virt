#include "proto/virtio_gpu/blob_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

BlobRequest::BlobRequest() : resource_id_(0u), offset_(0u)
{
    AV_SCOPE("BlobRequest::BlobRequest");
}

Status BlobRequest::decode_resource(WireReader &reader)
{
    AV_SCOPE("BlobRequest::decode_resource");
    if (reader.remaining() < resource_size) return Status::malformed;
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    resource_id_ = reader.read_u32();
    return Status::ok;
}

Status BlobRequest::decode_map(WireReader &reader)
{
    AV_SCOPE("BlobRequest::decode_map");
    if (reader.remaining() < map_size) return Status::malformed;
    const Status status = decode_resource(reader);
    static_cast<void>(reader.read_u32());
    offset_ = reader.read_u64();
    return status;
}

uint32_t BlobRequest::resource_id() const
{
    AV_SCOPE("BlobRequest::resource_id");
    return resource_id_;
}
uint64_t BlobRequest::offset() const
{
    AV_SCOPE("BlobRequest::offset");
    return offset_;
}

}  // namespace av
