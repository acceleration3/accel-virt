#include "proto/export/export_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ExportRequest::ExportRequest()
    : operation_(ExportOperation::hello), surface_(0u), tag_(0u), shift_ns_(0), flags_(0u)
{
    AV_SCOPE("ExportRequest::ExportRequest");
}

ExportRequest::ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag)
    : operation_(operation), surface_(surface), tag_(tag), shift_ns_(0), flags_(0u)
{
    AV_SCOPE("ExportRequest::ExportRequest");
}

ExportRequest::ExportRequest(int64_t shift_ns, uint64_t tag)
    : operation_(ExportOperation::timing), surface_(0u), tag_(tag), shift_ns_(shift_ns), flags_(0u)
{
    AV_SCOPE("ExportRequest::ExportRequest");
}

ExportRequest::ExportRequest(ExportOperation operation, uint64_t surface, uint64_t tag, uint32_t flags)
    : operation_(operation), surface_(surface), tag_(tag), shift_ns_(0), flags_(flags)
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

uint32_t ExportRequest::head() const
{
    AV_SCOPE("ExportRequest::head");
    return operation_ == ExportOperation::head ? static_cast<uint32_t>(surface_ - 1u) : 0u;
}

uint32_t ExportRequest::flags() const
{
    AV_SCOPE("ExportRequest::flags");
    return flags_;
}

bool ExportRequest::imports_on_other_gpu() const
{
    AV_SCOPE("ExportRequest::imports_on_other_gpu");
    return (flags_ & other_gpu) != 0u;
}

uint64_t ExportRequest::tag() const
{
    AV_SCOPE("ExportRequest::tag");
    return tag_;
}

int64_t ExportRequest::shift_ns() const
{
    AV_SCOPE("ExportRequest::shift_ns");
    return shift_ns_;
}

/* A timing's shift takes the surface's place, as a two's complement; an export
 * and a sync put their flags in the word at 12, and every other operation,
 * a timing among them, leaves it 0. */
Status ExportRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ExportRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(major);
    writer.write_u16(minor);
    writer.write_u32(static_cast<uint32_t>(operation_));
    writer.write_u32(flags_);
    writer.write_u64(operation_ == ExportOperation::timing ? static_cast<uint64_t>(shift_ns_) : surface_);
    writer.write_u64(tag_);
    return writer.status();
}

bool ExportRequest::fields_valid() const
{
    AV_SCOPE("ExportRequest::fields_valid");
    switch (operation_) {
    case ExportOperation::hello: return surface_ == 0u && flags_ == 0u;
    case ExportOperation::export_surface:
    case ExportOperation::sync: return surface_ != 0u && (flags_ & ~other_gpu) == 0u;
    case ExportOperation::release: return surface_ != 0u && flags_ == 0u;
    case ExportOperation::head: return surface_ != 0u && surface_ <= most_heads && flags_ == 0u;
    case ExportOperation::timing:
        return flags_ == 0u && shift_ns_ > -most_shift_ns && shift_ns_ < most_shift_ns;
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
    const uint32_t word = reader.read_u32();
    const uint64_t named = reader.read_u64();
    candidate.tag_ = reader.read_u64();
    candidate.flags_ = word;
    if (candidate.operation_ == ExportOperation::timing) candidate.shift_ns_ = static_cast<int64_t>(named);
    else candidate.surface_ = named;
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
