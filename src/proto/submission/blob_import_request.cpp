#include "proto/submission/blob_import_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImportRequest::BlobImportRequest() : resource_id_(0u)
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

BlobImportRequest::BlobImportRequest(uint32_t resource_id) : resource_id_(resource_id)
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

uint32_t BlobImportRequest::resource_id() const
{
    AV_SCOPE("BlobImportRequest::resource_id");
    return resource_id_;
}

Status BlobImportRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BlobImportRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(resource_id_);
    writer.write_zeros(4u);
    return writer.status();
}

Status BlobImportRequest::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("BlobImportRequest::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t resource_id = reader.read_u32();
    const uint32_t reserved = reader.read_u32();
    if (resource_id == 0u || reserved != 0u) return Status::invalid_parameter;
    resource_id_ = resource_id;
    return Status::ok;
}

}
