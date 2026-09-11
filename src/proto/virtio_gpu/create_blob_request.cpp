#include "proto/virtio_gpu/create_blob_request.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

CreateBlobRequest::CreateBlobRequest()
    : resource_id_(0u), memory_(0u), flags_(0u), entries_(0u), blob_id_(0u), size_(0u) {}

Status CreateBlobRequest::decode(WireReader &reader)
{
    if (reader.remaining() < fixed_size) return Status::malformed;
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    resource_id_ = reader.read_u32();
    memory_ = reader.read_u32();
    flags_ = reader.read_u32();
    entries_ = reader.read_u32();
    blob_id_ = reader.read_u64();
    size_ = reader.read_u64();
    return Status::ok;
}

uint32_t CreateBlobRequest::resource_id() const { return resource_id_; }
uint32_t CreateBlobRequest::memory() const { return memory_; }
uint32_t CreateBlobRequest::flags() const { return flags_; }
uint32_t CreateBlobRequest::entries() const { return entries_; }
uint64_t CreateBlobRequest::blob_id() const { return blob_id_; }
uint64_t CreateBlobRequest::size() const { return size_; }

}  // namespace av
