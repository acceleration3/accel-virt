#include "proto/virtio_gpu/blob_request.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

BlobRequest::BlobRequest() : resource_id_(0u), offset_(0u) {}

Status BlobRequest::decode_resource(WireReader &reader)
{
    if (reader.remaining() < resource_size) return Status::malformed;
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    resource_id_ = reader.read_u32();
    return Status::ok;
}

Status BlobRequest::decode_map(WireReader &reader)
{
    if (reader.remaining() < map_size) return Status::malformed;
    const Status status = decode_resource(reader);
    static_cast<void>(reader.read_u32());
    offset_ = reader.read_u64();
    return status;
}

uint32_t BlobRequest::resource_id() const { return resource_id_; }
uint64_t BlobRequest::offset() const { return offset_; }

}  // namespace av
