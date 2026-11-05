#include "proto/submission/blob_import_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImportRequest::BlobImportRequest() : resource_id_(0u), role_()
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

BlobImportRequest::BlobImportRequest(uint32_t resource_id) : resource_id_(resource_id), role_()
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

BlobImportRequest::BlobImportRequest(uint32_t resource_id, const BlobRole &role)
    : resource_id_(resource_id), role_(role)
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

uint32_t BlobImportRequest::resource_id() const
{
    AV_SCOPE("BlobImportRequest::resource_id");
    return resource_id_;
}

const BlobRole &BlobImportRequest::role() const
{
    AV_SCOPE("BlobImportRequest::role");
    return role_;
}

Status BlobImportRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BlobImportRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(resource_id_);
    writer.write_u32(static_cast<uint32_t>(role_.kind()));
    writer.write_u64(role_.base());
    return writer.status();
}

Status BlobImportRequest::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("BlobImportRequest::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t resource_id = reader.read_u32();
    const uint32_t kind = reader.read_u32();
    const uint64_t base = reader.read_u64();
    BlobRole       role;
    if (resource_id == 0u || role.decode(kind, base) != Status::ok) return Status::invalid_parameter;
    resource_id_ = resource_id;
    role_ = role;
    return Status::ok;
}

}
