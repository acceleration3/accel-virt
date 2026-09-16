#include "proto/submission/blob_import_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImportRequest::BlobImportRequest() : resource_id_(0u), offset_(0u), size_(0u), memory_type_(0u)
{
    AV_SCOPE("BlobImportRequest::BlobImportRequest");
}

BlobImportRequest::BlobImportRequest(uint32_t resource_id, uint64_t offset, uint64_t size, uint32_t memory_type)
    : resource_id_(resource_id), offset_(offset), size_(size), memory_type_(memory_type)
    {
        AV_SCOPE("BlobImportRequest::BlobImportRequest");
    }

uint32_t BlobImportRequest::resource_id() const
{
    AV_SCOPE("BlobImportRequest::resource_id");
    return resource_id_;
}
uint64_t BlobImportRequest::offset() const
{
    AV_SCOPE("BlobImportRequest::offset");
    return offset_;
}
uint64_t BlobImportRequest::size() const
{
    AV_SCOPE("BlobImportRequest::size");
    return size_;
}
uint32_t BlobImportRequest::memory_type() const
{
    AV_SCOPE("BlobImportRequest::memory_type");
    return memory_type_;
}

Status BlobImportRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BlobImportRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(resource_id_);
    writer.write_zeros(4u);
    writer.write_u64(offset_);
    writer.write_u64(size_);
    writer.write_u32(memory_type_);
    writer.write_zeros(4u);
    return writer.status();
}

bool BlobImportRequest::range_valid() const
{
    AV_SCOPE("BlobImportRequest::range_valid");
    if (size_ == 0u || offset_ % page_size != 0u || size_ % page_size != 0u) return false;
    return offset_ + size_ >= offset_;
}

Status BlobImportRequest::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("BlobImportRequest::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader        reader(bytes, length);
    BlobImportRequest candidate;
    candidate.resource_id_ = reader.read_u32();
    const uint32_t reserved = reader.read_u32();
    candidate.offset_ = reader.read_u64();
    candidate.size_ = reader.read_u64();
    candidate.memory_type_ = reader.read_u32();
    const uint32_t trailing = reader.read_u32();
    if (reserved != 0u || trailing != 0u || !candidate.range_valid()) return Status::invalid_parameter;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
