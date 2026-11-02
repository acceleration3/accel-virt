#include "proto/virtio_gpu/create_blob_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

CreateBlobRequest::CreateBlobRequest()
    : context_id_(0u), resource_id_(0u), memory_(0u), flags_(0u), entries_(0u), blob_id_(0u), size_(0u)
    {
        AV_SCOPE("CreateBlobRequest::CreateBlobRequest");
    }

Status CreateBlobRequest::decode(WireReader &reader)
{
    AV_SCOPE("CreateBlobRequest::decode");
    if (reader.remaining() < fixed_size) return Status::malformed;
    reader.skip(header_before_context);
    context_id_ = reader.read_u32();
    reader.skip(header_after_context);
    resource_id_ = reader.read_u32();
    memory_ = reader.read_u32();
    flags_ = reader.read_u32();
    entries_ = reader.read_u32();
    blob_id_ = reader.read_u64();
    size_ = reader.read_u64();
    return Status::ok;
}

uint32_t CreateBlobRequest::context_id() const
{
    AV_SCOPE("CreateBlobRequest::context_id");
    return context_id_;
}

uint32_t CreateBlobRequest::resource_id() const
{
    AV_SCOPE("CreateBlobRequest::resource_id");
    return resource_id_;
}
uint32_t CreateBlobRequest::memory() const
{
    AV_SCOPE("CreateBlobRequest::memory");
    return memory_;
}
uint32_t CreateBlobRequest::flags() const
{
    AV_SCOPE("CreateBlobRequest::flags");
    return flags_;
}
uint32_t CreateBlobRequest::entries() const
{
    AV_SCOPE("CreateBlobRequest::entries");
    return entries_;
}
uint64_t CreateBlobRequest::blob_id() const
{
    AV_SCOPE("CreateBlobRequest::blob_id");
    return blob_id_;
}
uint64_t CreateBlobRequest::size() const
{
    AV_SCOPE("CreateBlobRequest::size");
    return size_;
}

}
