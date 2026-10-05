#include "proto/export/export_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ExportRequest::ExportRequest() : operation_(ExportOperation::hello), surface_(0u), tag_(0u)
{
    AV_SCOPE("ExportRequest::ExportRequest");
}

ExportRequest::ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag)
    : operation_(operation), surface_(surface), tag_(tag)
{
    AV_SCOPE("ExportRequest::ExportRequest");
}

ExportOperation ExportRequest::operation() const
{
    AV_SCOPE("ExportRequest::operation");
    return operation_;
}

uint64_t ExportRequest::surface() const
{
    AV_SCOPE("ExportRequest::surface");
    return surface_;
}

uint64_t ExportRequest::tag() const
{
    AV_SCOPE("ExportRequest::tag");
    return tag_;
}

Status ExportRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ExportRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(major);
    writer.write_u16(minor);
    writer.write_u32(static_cast<uint32_t>(operation_));
    writer.write_zeros(4u);
    writer.write_u64(surface_);
    writer.write_u64(tag_);
    return writer.status();
}

bool ExportRequest::fields_valid() const
{
    AV_SCOPE("ExportRequest::fields_valid");
    switch (operation_) {
    case ExportOperation::hello: return surface_ == 0u;
    case ExportOperation::export_surface:
    case ExportOperation::sync:
    case ExportOperation::release: return surface_ != 0u;
    default: return false;
    }
}

Status ExportRequest::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ExportRequest::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_major = reader.read_u16();
    reader.skip(sizeof(uint16_t));
    if (found_magic != magic) return Status::malformed;
    if (found_major != major) return Status::not_supported;
    ExportRequest candidate;
    candidate.operation_ = static_cast<ExportOperation>(reader.read_u32());
    const uint32_t reserved = reader.read_u32();
    candidate.surface_ = reader.read_u64();
    candidate.tag_ = reader.read_u64();
    if (reserved != 0u || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
